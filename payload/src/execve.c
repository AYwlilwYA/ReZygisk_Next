/**
 * execve.c - execve 系统调用的独立实现
 *
 * 当需要从非内联函数调用 execve 时使用此文件。
 * 大多数情况下使用 syscall.h 中的内联版本即可。
 *
 * 此文件确保即使在内联被禁用时也能正常工作。
 */

#include <asm/unistd.h>

#ifdef __aarch64__
/* 注意: 不使用 naked 函数 + 扩展 asm (违反 GCC 规范)。
 * 使用 register local 变量方式，与 syscall.h 中一致。 */
long __payload_execve(const char *path, char *const argv[],
                      char *const envp[]) {
    register long x8 __asm__("x8") = __NR_execve;
    register long x0 __asm__("x0") = (long)path;
    register long x1 __asm__("x1") = (long)argv;
    register long x2 __asm__("x2") = (long)envp;

    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory"
    );

    if (x0 < 0) return x0;
    return 0;
}
#endif
