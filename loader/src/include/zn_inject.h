/**
 * zn_inject.h — ReZygisk Next 进程注入接口
 *
 * 将 libzygisk.so 注入到 Zygote 进程。
 * 优先使用 process_vm_writev，回退到 ptrace。
 */

#ifndef ZN_INJECT_H
#define ZN_INJECT_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * zn_inject_library — 将共享库注入到目标进程
 *
 * 使用最佳可用方法将 .so 文件注入到目标进程的地址空间。
 *
 * 策略优先级:
 * 1. process_vm_writev + mremap（无痕迹，首选）
 * 2. process_vm_writev + mmap（较隐蔽）
 * 3. /proc/pid/mem 直接写入（回退方案）
 * 4. ptrace 注入（最后手段）
 *
 * @param target_pid  目标进程 PID
 * @param lib_path    要注入的库的路径（磁盘上的 .so 文件）
 * @param entry_sym   入口符号名称（如 "zygisk_entry"）
 * @param base_out    输出：注入后的基地址
 * @param size_out    输出：注入的库大小
 * @param entry_out   输出：入口函数地址
 * @return            成功返回 true
 */
bool zn_inject_library(pid_t target_pid, const char *lib_path,
                        const char *entry_sym,
                        void **base_out, size_t *size_out,
                        void **entry_out);

/**
 * zn_inject_payload — 将 payload 代码注入到目标进程
 *
 * 注入最小化的纯 syscall 代码（类似 libpayload.so）。
 * 用于在启动 zygiskd 之前的早期引导。
 *
 * @param target_pid  目标进程 PID
 * @param payload     载荷代码
 * @param payload_len 载荷代码长度
 * @param entry_addr  载荷入口地址
 * @return            成功返回 true
 */
bool zn_inject_payload(pid_t target_pid,
                        const void *payload, size_t payload_len,
                        void *entry_addr);

#endif /* ZN_INJECT_H */
