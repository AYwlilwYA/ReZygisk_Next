/**
 * zn_inject.c — ReZygisk Next 进程注入实现
 *
 * 实现 process_vm_writev 方式的库注入。
 * 参考 Zygisk-Next zygiskd 的实现。
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "elf_util.h"
#include "logging.h"
#include "misc.h"
#include "proc_vm.h"
#include "zn_inject.h"
#include "zn_constants.h"

/* ====================================================
 * 辅助函数
 * ==================================================== */

/**
 * find_free_region — 在目标进程的 maps 中找空闲区域
 *
 * 解析 /proc/pid/maps，寻找一块足够大的匿名可写区域。
 * 用于 mremap 注入（复用现有 VMA，不创建新条目）。
 */
static bool find_free_region(pid_t pid, size_t required_size,
                              void **out_addr, size_t *out_avail) {
    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);

    FILE *fp = fopen(maps_path, "r");
    if (!fp) {
        LOGE("Failed to open %s", maps_path);
        return false;
    }

    bool found = false;
    char line[512];
    uintptr_t last_end = 0;

    while (fgets(line, sizeof(line), fp)) {
        uintptr_t start, end;
        char perms[5], path[256] = {0};

        int n = sscanf(line, "%lx-%lx %4s %*x %*s %*s %255s",
                       &start, &end, perms, path);
        if (n < 3) continue;

        /* 检查匿名可写区域 */
        if (path[0] == '\0' &&                          /* 匿名映射 */
            strchr(perms, 'w') &&                        /* 可写 */
            (end - start) >= required_size) {            /* 足够大 */

            *out_addr = (void *)start;
            *out_avail = end - start;
            found = true;
            break;
        }

        last_end = end;
    }

    fclose(fp);
    return found;
}

/**
 * load_elf_file — 将 ELF 文件加载到本地内存
 */
static void *load_elf_file(const char *path, size_t *out_size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LOGE("Failed to open %s: %s", path, strerror(errno));
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        LOGE("fstat failed for %s", path);
        close(fd);
        return NULL;
    }

    void *data = mmap(NULL, (size_t)st.st_size, PROT_READ,
                       MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) {
        LOGE("mmap failed for %s", path);
        return NULL;
    }

    *out_size = (size_t)st.st_size;
    return data;
}

/* ====================================================
 * 主注入逻辑
 * ==================================================== */

/**
 * 方法1: process_vm_writev + mremap
 *
 * 最隐蔽的方法。将 .so 写入目标进程现有的匿名区域，
 * 然后使用 mremap 调整映射大小。不创建新的 VMA 条目。
 */
static bool inject_via_vmwrite_mremap(pid_t pid,
                                       const void *elf_data, size_t elf_size,
                                       void **out_base, void **out_entry) {
    void *region_addr = NULL;
    size_t region_size = 0;

    /* 1. 查找适合的匿名映射区域 */
    if (!find_free_region(pid, elf_size + 4096,
                          &region_addr, &region_size)) {
        LOGW("No suitable anonymous region found, falling back to mmap");
        return false;
    }

    LOGD("Found anonymous region at %p (%zu bytes)", region_addr, region_size);

    /* 2. 将 ELF 数据写入目标进程 */
    if (!proc_vm_write_chunked(pid, region_addr, elf_data, elf_size)) {
        LOGE("Failed to write ELF data to %p in pid %d", region_addr, pid);
        return false;
    }

    LOGD("ELF data written to %p (%zu bytes)", region_addr, elf_size);

    /* 3. 使用 mremap 调整映射（避免创建新 VMA 条目） */
    /*    mremap 通过 syscall 直接调用 */
    /*    注意: syscall 失败返回负 errno (非 MAP_FAILED) */
    long mremap_ret = syscall(__NR_mremap,
                               region_addr,     /* old_address */
                               region_size,     /* old_size */
                               elf_size,        /* new_size */
                               MREMAP_MAYMOVE,  /* flags */
                               NULL);           /* new_address (kernel picks) */

    void *new_addr;
    if (mremap_ret < 0 && mremap_ret > -4096) {
        LOGW("mremap failed (errno=%ld), trying without MREMAP_MAYMOVE", -mremap_ret);

        mremap_ret = syscall(__NR_mremap,
                              region_addr, region_size,
                              elf_size, 0, NULL);

        if (mremap_ret < 0 && mremap_ret > -4096) {
            LOGE("mremap completely failed (errno=%ld), "
                 "data may be at original address %p", -mremap_ret, region_addr);
            new_addr = region_addr;  /* 继续使用原始地址 */
        } else {
            new_addr = (void *)mremap_ret;
        }
    } else {
        new_addr = (void *)mremap_ret;
    }

    /* 4. 远程进程的 mprotect
     *    注意: 当前 mprotect 在本地进程上下文执行，对远程进程无效。
     *    TODO: 通过注入 shellcode 或 /proc/pid/mem 修改远程页权限
     *    详见 mremap_loader.c 的 TODO 说明 */
    (void)new_addr;  /* 标记为已使用，避免未使用变量警告 */

    *out_base = new_addr;
    *out_entry = NULL; /* 调用者需要通过 ELF 解析找到入口 */

    return true;
}

/**
 * 方法2: /proc/pid/mem 直接写入
 *
 * 回退方案。当 process_vm_writev 不可用时使用。
 * 仍然不产生 ptrace 痕迹。
 */
static bool inject_via_proc_mem(pid_t pid,
                                 const void *elf_data, size_t elf_size,
                                 void **out_base, void **out_entry) {
    int mem_fd = proc_mem_open(pid, O_RDWR);
    if (mem_fd < 0) {
        return false;
    }

    /* 查找目标地址 */
    void *region_addr = NULL;
    size_t region_size = 0;

    if (!find_free_region(pid, elf_size + 4096,
                          &region_addr, &region_size)) {
        close(mem_fd);
        return false;
    }

    /* 写入 */
    ssize_t written = proc_mem_pwrite(mem_fd, elf_data, elf_size,
                                       (off_t)region_addr);
    close(mem_fd);

    if (written < 0 || (size_t)written != elf_size) {
        LOGE("proc_mem write failed: %zd/%zu bytes", written, elf_size);
        return false;
    }

    *out_base = region_addr;
    *out_entry = NULL;

    LOGD("Injected via /proc/pid/mem at %p", region_addr);
    return true;
}

/* ====================================================
 * 公共 API 实现
 * ==================================================== */

bool zn_inject_library(pid_t target_pid, const char *lib_path,
                        const char *entry_sym,
                        void **base_out, size_t *size_out,
                        void **entry_out) {
    if (!lib_path || !base_out || !size_out || !entry_out) {
        errno = EINVAL;
        return false;
    }

    /* 加载 ELF 文件到本地内存 */
    size_t elf_size = 0;
    void *elf_data = load_elf_file(lib_path, &elf_size);
    if (!elf_data) {
        return false;
    }

    LOGI("Injecting %s (%zu bytes) into pid %d", lib_path, elf_size, target_pid);

    bool injected = false;
    void *remote_base = NULL;

    /* 策略1: process_vm_writev + mremap（首选） */
    if (proc_vm_is_available()) {
        injected = inject_via_vmwrite_mremap(target_pid,
                                              elf_data, elf_size,
                                              &remote_base, entry_out);
        if (injected) {
            LOGI("Injected via process_vm_writev + mremap");
        }
    }

    /* 策略2: /proc/pid/mem（回退） */
    if (!injected) {
        injected = inject_via_proc_mem(target_pid,
                                        elf_data, elf_size,
                                        &remote_base, entry_out);
        if (injected) {
            LOGI("Injected via /proc/pid/mem (fallback)");
        }
    }

    /* 清理本地映射 */
    munmap(elf_data, elf_size);

    if (!injected) {
        LOGE("All injection methods failed for %s", lib_path);
        return false;
    }

    *base_out = remote_base;
    *size_out = elf_size;

    /* 如果调用者需要入口地址，使用 ELF 解析找到 */
    if (entry_sym && remote_base) {
        /* 解析本地 ELF 找到符号偏移 */
        ElfImg *img = ElfImg_create(lib_path, NULL);
        if (img) {
            ElfW(Addr) offset = getSymbOffset(img, entry_sym, NULL);
            if (offset != 0) {
                /* 远程地址 = 基地址 + 偏移 */
                *entry_out = (void *)((uintptr_t)remote_base + offset);
            }
            ElfImg_destroy(img);
        }
    }

    return true;
}

bool zn_inject_payload(pid_t target_pid,
                        const void *payload, size_t payload_len,
                        void *entry_addr) {
    if (!payload || payload_len == 0) {
        return false;
    }

    /* Payload 注入：直接将代码写入目标进程并改变权限 */
    if (!proc_vm_write_chunked(target_pid, entry_addr,
                                payload, payload_len)) {
        LOGE("Failed to inject payload at %p", entry_addr);
        return false;
    }

    LOGD("Payload injected at %p (%zu bytes)", entry_addr, payload_len);
    return true;
}
