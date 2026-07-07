/**
 * wait4.c - wait4 系统调用的独立实现
 */

#include <asm/unistd.h>
#include <sys/resource.h>
#include <sys/types.h>

#ifdef __aarch64__
/* 注意: 不使用 naked 函数 + 扩展 asm (违反 GCC 规范)。
 * 使用 register local 变量方式，与 syscall.h 中一致。 */
long __payload_wait4(pid_t pid, int *status, int options,
                      struct rusage *rusage) {
    register long x8 __asm__("x8") = __NR_wait4;
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

    if (x0 < 0) return -1;
    return (pid_t)x0;
}
#endif
