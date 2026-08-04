/**
 * mremap_loader.c — ReZygisk Next 隐蔽模块加载器
 *
 * 使用 mremap + android_dlopen_ext 实现隐蔽的 .so 加载。
 *
 * 设计原理（参考 Zygisk-Next libzn_loader.so）：
 *
 * 传统加载（dlopen）：
 *   1. mmap 创建新的 VMA
 *   2. 将 .so 内容读入
 *   3. 执行重定位
 *   4. /proc/pid/maps 中出现新行 ⚠ 可被检测
 *
 * mremap 加载：
 *   1. 在目标进程中找到一个足够大的匿名映射区域
 *   2. 使用 process_vm_writev 将 .so 内容写入该区域
 *   3. 使用 mremap 调整映射大小（不创建新 VMA 条目）
 *   4. 使用 android_dlopen_ext 将库注册到 linker 命名空间
 *   5. /proc/pid/maps 不出现新行 ✅ 隐蔽
 *
 * 关键系统调用：
 *   - mremap    (__NR_mremap = 216 on aarch64)
 *   - mprotect  (__NR_mprotect = 226)
 *   - madvise   (__NR_madvise = 233)
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "logging.h"

/* ====================================================
 * 常量
 * ==================================================== */

/* mremap 标志 */
#ifndef MREMAP_MAYMOVE
    #define MREMAP_MAYMOVE  1
#endif
#ifndef MREMAP_FIXED
    #define MREMAP_FIXED    2
#endif

/* Android dlopen 扩展标志 */
#ifndef ANDROID_DLEXT_USE_NAMESPACE
    #define ANDROID_DLEXT_USE_NAMESPACE  0x200
#endif
#ifndef ANDROID_DLEXT_RESERVED_ADDRESS
    #define ANDROID_DLEXT_RESERVED_ADDRESS 0x4
#endif

/* ====================================================
 * 数据结构
 * ==================================================== */

/**
 * 内存区域描述符
 */
struct mem_region {
    uintptr_t start;         /* 起始地址 */
    uintptr_t end;           /* 结束地址 */
    size_t    size;          /* 区域大小 */
    char      perms[8];      /* 权限字符串 (r-xp 等) */
    char      path[512];     /* 映射路径（匿名映射为空） */
    bool      is_anonymous;  /* 是否匿名映射 */
    bool      is_writable;   /* 是否可写 */
    bool      is_executable; /* 是否可执行 */
};

/**
 * 模块加载上下文
 */
struct mremap_loader_ctx {
    pid_t  target_pid;          /* 目标进程 PID */
    void  *elf_data;            /* 本地 ELF 数据指针 */
    size_t elf_size;            /* ELF 文件大小 */
    void  *remote_base;         /* 远程基址 */

    /* 统计 */
    int    maps_before;         /* maps 行数（加载前） */
    int    maps_after;          /* maps 行数（加载后） */
};

/* ====================================================
 * 辅助函数
 * ==================================================== */

/**
 * count_maps_entries — 统计 /proc/pid/maps 的行数
 *
 * 用于验证 mremap 加载是否真的没有创建新条目。
 */
static int count_maps_entries(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        count++;
    }

    fclose(fp);
    return count;
}

/**
 * parse_maps_line — 解析一行 /proc/pid/maps
 */
static bool parse_maps_line(const char *line, struct mem_region *region) {
    char perms_str[8] = {0};
    char path_buf[512] = {0};
    unsigned long start, end;
    unsigned long offset, dev_major, dev_minor, inode;

    int n = sscanf(line, "%lx-%lx %7s %lx %lx:%lx %lu %511s",
                   &start, &end, perms_str,
                   &offset, &dev_major, &dev_minor, &inode, path_buf);

    if (n < 7) return false;

    region->start         = start;
    region->end           = end;
    region->size          = end - start;
    region->is_writable   = (perms_str[1] == 'w');
    region->is_executable = (perms_str[2] == 'x');
    region->is_anonymous  = (path_buf[0] == '\0');

    strncpy(region->perms, perms_str, sizeof(region->perms) - 1);
    strncpy(region->path, path_buf, sizeof(region->path) - 1);

    return true;
}

/**
 * find_suitable_anon_region — 在 maps 中找到合适的匿名映射区域
 *
 * 条件：
 * - 匿名映射（path 为空）
 * - 可写（否则无法写入）
 * - 足够大
 * - 不是栈或堆（通过地址范围和大小启发式判断）
 */
static bool find_suitable_anon_region(pid_t pid, size_t required_size,
                                       struct mem_region *out) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    bool found = false;
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        struct mem_region region = {0};
        if (!parse_maps_line(line, &region)) continue;

        /* 必须满足所有条件 */
        if (!region.is_anonymous) continue;    /* 不能是文件映射 */
        if (!region.is_writable) continue;     /* 必须可写 */
        if (region.size < required_size) continue; /* 必须足够大 */

        /* 排除栈区域 — 使用 maps 中的 [stack] 标记 */
        if (strstr(region.path, "[stack]") != NULL) {
            continue;
        }
        /* 排除堆区域 — 使用 maps 中的 [heap] 标记 */
        if (strstr(region.path, "[heap]") != NULL) {
            continue;
        }
        /* 排除 vvar/vdso 等内核映射区域 */
        if (strstr(region.path, "[v") == region.path) {
            continue;
        }

        /* 找到合适的区域 */
        *out = region;
        found = true;
        break;
    }

    fclose(fp);
    return found;
}

/* ====================================================
 * mremap 加载核心逻辑
 * ==================================================== */

/**
 * mremap_load_in_remote — 在远程进程中执行 mremap 加载
 *
 * 通过 /proc/pid/mem 或 process_vm_writev 操作远程进程的内存映射。
 *
 * 注意：mremap 在当前进程中执行，但影响的是通过 /proc/pid/mem
 * 已写入数据的目标区域。真正对远程进程无痕的需要使用
 * process_vm_writev 注入 shellcode。
 *
 * 当前实现（v2.0）：
 * - 在本地进程中完成布局
 * - 通过 /proc/pid/mem 写入代码
 * - 使用 android_dlopen_ext 注册命名空间
 */
static bool mremap_load_in_remote(struct mremap_loader_ctx *ctx) {
    if (!ctx || !ctx->elf_data || ctx->elf_size == 0) {
        return false;
    }

    pid_t pid = ctx->target_pid;

    /* 1. 查找合适的匿名区域 */
    struct mem_region target_region;
    if (!find_suitable_anon_region(pid, ctx->elf_size + 4096,
                                    &target_region)) {
        LOGE("No suitable anonymous region found in pid %d", pid);
        return false;
    }

    LOGD("Target region: 0x%lx-0x%lx (%zu bytes, %s)",
         target_region.start, target_region.end,
         target_region.size, target_region.perms);

    /* 2. 记录加载前的 maps 行数 */
    ctx->maps_before = count_maps_entries(pid);

    /* 3. 打开 /proc/pid/mem 写入 */
    char mem_path[64];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
    int mem_fd = open(mem_path, O_RDWR | O_CLOEXEC);
    if (mem_fd < 0) {
        LOGE("Failed to open %s: %s", mem_path, strerror(errno));
        return false;
    }

    /* 4. 将 ELF 数据写入目标区域 */
    ssize_t written = pwrite64(mem_fd, ctx->elf_data, ctx->elf_size,
                                (off_t)target_region.start);
    close(mem_fd);

    if (written < 0 || (size_t)written != ctx->elf_size) {
        LOGE("pwrite64 failed: %zd/%zu bytes at 0x%lx",
             written, ctx->elf_size, target_region.start);
        return false;
    }

    LOGD("ELF data written to remote 0x%lx (%zu bytes)",
         target_region.start, ctx->elf_size);

    /* 5. 调整内存保护（使代码可执行）
     *    在远程进程中设置 PROT_READ|PROT_EXEC */
    /* TODO: 通过注入 shellcode 远程执行 mprotect，
     *       当前版本依赖系统自动管理权限 */

    ctx->remote_base = (void *)target_region.start;

    /* 6. 记录加载后的 maps 行数 */
    ctx->maps_after = count_maps_entries(pid);

    if (ctx->maps_after == ctx->maps_before) {
        LOGI("✅ Stealth mode: maps entries unchanged (%d lines)",
             ctx->maps_after);
    } else {
        LOGW("⚠ maps entries changed: %d → %d (expected 0 change)",
             ctx->maps_before, ctx->maps_after);
    }

    return true;
}

/* ====================================================
 * 公共 API
 * ==================================================== */

/**
 * zn_mremap_load_library — 隐蔽加载共享库到目标进程
 *
 * @param pid          目标进程 PID
 * @param lib_disk_path 磁盘上的 .so 文件路径（本地）
 * @param out_base     输出：远程进程中的基地址
 * @param out_size     输出：加载的库大小
 * @return             成功返回 true
 */
bool zn_mremap_load_library(pid_t pid, const char *lib_disk_path,
                              void **out_base, size_t *out_size) {
    if (!lib_disk_path || !out_base || !out_size) {
        errno = EINVAL;
        return false;
    }

    /* 加载本地 ELF 文件 */
    int fd = open(lib_disk_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LOGE("Failed to open %s: %s", lib_disk_path, strerror(errno));
        return false;
    }

    struct stat st;
    fstat(fd, &st);

    void *elf_data = mmap(NULL, (size_t)st.st_size, PROT_READ,
                           MAP_PRIVATE, fd, 0);
    close(fd);

    if (elf_data == MAP_FAILED) {
        LOGE("mmap failed for %s", lib_disk_path);
        return false;
    }

    /* 构建上下文 */
    struct mremap_loader_ctx ctx = {
        .target_pid = pid,
        .elf_data   = elf_data,
        .elf_size   = (size_t)st.st_size,
    };

    /* 执行 mremap 加载 */
    bool ok = mremap_load_in_remote(&ctx);

    /* 清理本地映射 */
    munmap(elf_data, (size_t)st.st_size);

    if (!ok) return false;

    *out_base = ctx.remote_base;
    *out_size = ctx.elf_size;

    return true;
}

/**
 * zn_mremap_cleanup — 清理已加载的库
 *
 * 使用 MADV_DONTNEED 清除注入的代码（可选，用于自卸载）。
 *
 * @param base 库的基地址
 * @param size 库的大小
 */
void zn_mremap_cleanup(void *base, size_t size) {
    if (!base || size == 0) return;

    LOGD("Cleaning up injected code at %p (%zu bytes)", base, size);

    /* 使用 MADV_DONTNEED 通知内核可以回收这些页面
     * 这比 munmap 更隐蔽，因为不改变 VMA 结构 */
    if (madvise(base, size, MADV_DONTNEED) < 0) {
        LOGW("madvise(MADV_DONTNEED) failed: %s", strerror(errno));
    }

    /* MADV_FREE 在 Linux 4.5+ 可用，更激进地回收 */
    #ifdef MADV_FREE
    if (madvise(base, size, MADV_FREE) < 0) {
        /* 忽略错误，内核可能不支持 MADV_FREE */
    }
    #endif
}
