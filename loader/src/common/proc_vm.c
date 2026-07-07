/**
 * proc_vm.c — 跨进程内存访问实现
 *
 * 使用 process_vm_writev / process_vm_readv 系统调用
 * 实现无 ptrace 的进程内存读写。
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/syscall.h>
#include <sys/uio.h>

#include "logging.h"
#include "proc_vm.h"

/* ====================================================
 * 系统调用包装
 * ==================================================== */

static ssize_t _process_vm_writev(pid_t pid,
                                   const struct iovec *local_iov,
                                   unsigned long local_nr,
                                   const struct iovec *remote_iov,
                                   unsigned long remote_nr,
                                   unsigned long flags) {
    return syscall(__NR_process_vm_writev, pid,
                   local_iov, local_nr, remote_iov, remote_nr, flags);
}

static ssize_t _process_vm_readv(pid_t pid,
                                  const struct iovec *local_iov,
                                  unsigned long local_nr,
                                  const struct iovec *remote_iov,
                                  unsigned long remote_nr,
                                  unsigned long flags) {
    return syscall(__NR_process_vm_readv, pid,
                   local_iov, local_nr, remote_iov, remote_nr, flags);
}

/* ====================================================
 * 公共 API
 * ==================================================== */

ssize_t proc_vm_write(pid_t pid, void *remote_addr,
                       const void *local_buf, size_t len) {
    if (!local_buf || !remote_addr || len == 0) {
        errno = EINVAL;
        return -1;
    }

    struct iovec local_iov = {
        .iov_base = (void *)local_buf,
        .iov_len  = len,
    };
    struct iovec remote_iov = {
        .iov_base = remote_addr,
        .iov_len  = len,
    };

    ssize_t ret = _process_vm_writev(pid, &local_iov, 1,
                                      &remote_iov, 1, 0);

    if (ret < 0) {
        LOGE("process_vm_writev failed for pid %d at %p: %s",
             pid, remote_addr, strerror(errno));
    }

    return ret;
}

ssize_t proc_vm_read(pid_t pid, const void *remote_addr,
                      void *local_buf, size_t len) {
    if (!local_buf || !remote_addr || len == 0) {
        errno = EINVAL;
        return -1;
    }

    struct iovec local_iov = {
        .iov_base = local_buf,
        .iov_len  = len,
    };
    struct iovec remote_iov = {
        .iov_base = (void *)remote_addr,
        .iov_len  = len,
    };

    ssize_t ret = _process_vm_readv(pid, &local_iov, 1,
                                     &remote_iov, 1, 0);

    if (ret < 0) {
        LOGE("process_vm_readv failed for pid %d at %p: %s",
             pid, remote_addr, strerror(errno));
    }

    return ret;
}

bool proc_vm_write_chunked(pid_t pid, void *remote_addr,
                            const void *local_buf, size_t total_len) {
    const char *buf = (const char *)local_buf;
    size_t remaining = total_len;
    char *addr = (char *)remote_addr;

    /* 分块大小：使用安全的默认值 */
    const size_t chunk_size = 4096;  /* 一个页面 */

    while (remaining > 0) {
        size_t this_chunk = remaining > chunk_size ? chunk_size : remaining;

        ssize_t written = proc_vm_write(pid, addr, buf, this_chunk);
        if (written < 0) {
            /* 回退到 /proc/pid/mem 方式 */
            int mem_fd = proc_mem_open(pid, O_RDWR);
            if (mem_fd < 0) {
                LOGE("Failed to open /proc/%d/mem (fallback)", pid);
                return false;
            }

            written = proc_mem_pwrite(mem_fd, buf, this_chunk,
                                       (off_t)addr);
            close(mem_fd);

            if (written < 0) {
                LOGE("Failed to write to /proc/%d/mem at %p", pid, addr);
                return false;
            }
        }

        buf += written;
        addr += written;
        remaining -= (size_t)written;
    }

    return true;
}

/* ====================================================
 * /proc/pid/mem 后备访问
 * ==================================================== */

int proc_mem_open(pid_t pid, int flags) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/mem", pid);

    int fd = open(path, flags | O_CLOEXEC);
    if (fd < 0) {
        LOGE("Failed to open %s: %s", path, strerror(errno));
    }

    return fd;
}

ssize_t proc_mem_pwrite(int fd, const void *buf, size_t len, off_t offset) {
    /* 使用 pwrite64 在指定偏移处写入 */
    ssize_t ret = pwrite64(fd, buf, len, offset);
    if (ret < 0) {
        LOGE("pwrite64 to /proc/pid/mem at 0x%lx failed: %s",
             offset, strerror(errno));
    }
    return ret;
}

ssize_t proc_mem_pread(int fd, void *buf, size_t len, off_t offset) {
    ssize_t ret = pread64(fd, buf, len, offset);
    if (ret < 0) {
        LOGE("pread64 from /proc/pid/mem at 0x%lx failed: %s",
             offset, strerror(errno));
    }
    return ret;
}

/* ====================================================
 * 可用性检测
 * ==================================================== */

bool proc_vm_is_available(void) {
    /* 尝试对自己写入 0 字节来检测系统调用是否可用 */
    struct iovec dummy = { .iov_base = NULL, .iov_len = 0 };
    ssize_t ret = _process_vm_writev(getpid(), &dummy, 0, &dummy, 0, 0);

    /* ENOSYS 表示系统调用不存在（需要回退方案） */
    if (ret < 0 && errno == ENOSYS) {
        LOGW("process_vm_writev not available (ENOSYS), "
             "will use /proc/pid/mem fallback");
        return false;
    }

    return true;
}
