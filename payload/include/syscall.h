/**
 * syscall.h - ReZygisk Next Payload 系统调用封装
 *
 * 纯内核系统调用，零 libc 依赖。
 * 参考 Zygisk-Next libpayload.so 中的 my_execve / my_wait4 实现。
 */

#ifndef PAYLOAD_SYSCALL_H
#define PAYLOAD_SYSCALL_H

#ifdef __aarch64__

/**
 * payload_execve - 执行程序 (纯 syscall 实现)
 *
 * 直接调用 Linux __NR_execve (221)，不经过 libc。
 *
 * @param path 可执行文件路径
 * @param argv 参数数组 (NULL 终止)
 * @param envp 环境变量数组 (NULL 终止，可传 NULL)
 * @return 成功不返回；失败返回负 errno
 */
static inline long payload_execve(const char *path, char *const argv[],
                                   char *const envp[]) {
    register long x8 __asm__("x8") = __NR_execve;  // 221
    register long x0 __asm__("x0") = (long)path;
    register long x1 __asm__("x1") = (long)argv;
    register long x2 __asm__("x2") = (long)envp;

    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory"
    );

    if (x0 < 0) return x0;  /* 返回负 errno */
    return 0;               /* execve 成功不返回，此行不可达 */
}

/**
 * payload_wait4 - 等待子进程 (纯 syscall 实现)
 *
 * 直接调用 Linux __NR_wait4 (260)，不经过 libc。
 *
 * @param pid     等待的进程 PID
 * @param status  输出：子进程退出状态
 * @param options WNOHANG / WUNTRACED 等
 * @param rusage  资源使用统计 (可传 NULL)
 * @return >0: 状态改变的 PID; 0: WNOHANG 无子进程退出; <0: 错误
 */
static inline long payload_wait4(pid_t pid, int *status, int options,
                                  struct rusage *rusage) {
    register long x8 __asm__("x8") = __NR_wait4;  // 260
    register long x0 __asm__("x0") = (long)pid;
    register long x1 __asm__("x1") = (long)status;
    register long x2 __asm__("x2") = (long)options;
    register long x3 __asm__("x3") = (long)rusage;

    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
        : "memory"
    );

    if (x0 < 0) return -1;   /* 错误 */
    return (pid_t)x0;        /* 返回状态改变的进程 PID */
}

#elif defined(__arm__)

/* arm32 版本 - 使用 swi #0 */
static inline long payload_execve(const char *path, char *const argv[],
                                   char *const envp[]) {
    register long r7 __asm__("r7") = __NR_execve;  /* 11 */
    register long r0 __asm__("r0") = (long)path;
    register long r1 __asm__("r1") = (long)argv;
    register long r2 __asm__("r2") = (long)envp;

    __asm__ volatile(
        "swi #0"
        : "+r"(r0)
        : "r"(r7), "r"(r1), "r"(r2)
        : "memory"
    );

    if (r0 < 0) return r0;
    return 0;
}

static inline long payload_wait4(pid_t pid, int *status, int options,
                                  struct rusage *rusage) {
    register long r7 __asm__("r7") = __NR_wait4;  /* 114 */
    register long r0 __asm__("r0") = (long)pid;
    register long r1 __asm__("r1") = (long)status;
    register long r2 __asm__("r2") = (long)options;
    register long r3 __asm__("r3") = (long)rusage;

    __asm__ volatile(
        "swi #0"
        : "+r"(r0)
        : "r"(r7), "r"(r1), "r"(r2), "r"(r3)
        : "memory"
    );

    if (r0 < 0) return -1;
    return (pid_t)r0;
}

#elif defined(__i386__)

/* x86 32-bit 版本 */
static inline long payload_execve(const char *path, char *const argv[],
                                   char *const envp[]) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(__NR_execve), "b"(path), "c"(argv), "d"(envp)
        : "memory"
    );
    if (ret < 0) return ret;
    return 0;
}

static inline long payload_wait4(pid_t pid, int *status, int options,
                                  struct rusage *rusage) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(__NR_wait4), "b"(pid), "c"(status), "d"(options), "S"(rusage)
        : "memory"
    );
    if (ret < 0) return -1;
    return (pid_t)ret;
}

#elif defined(__x86_64__)

/* x86_64 版本 */
static inline long payload_execve(const char *path, char *const argv[],
                                   char *const envp[]) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(__NR_execve), "D"(path), "S"(argv), "d"(envp)
        : "rcx", "r11", "memory"
    );
    if (ret < 0) return ret;
    return 0;
}

static inline long payload_wait4(pid_t pid, int *status, int options,
                                  struct rusage *rusage) {
    register long r10 __asm__("r10") = (long)rusage;
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(__NR_wait4), "D"(pid), "S"(status), "d"(options), "r"(r10)
        : "rcx", "r11", "memory"
    );
    if (ret < 0) return -1;
    return (pid_t)ret;
}

#endif /* arch */

#endif /* PAYLOAD_SYSCALL_H */
