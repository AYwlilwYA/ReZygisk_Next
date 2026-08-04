/**
 * zn_stealth.h - ReZygisk Next zygiskd 隐藏性增强
 *
 * 参考 Zygisk-Next zygiskd 的隐藏技术：
 * - 进程名伪装 (prctl PR_SET_NAME)
 * - 不设置 dumpable (prctl PR_SET_DUMPABLE)
 * - SELinux 上下文管理
 * - 反 ptrace 附加
 * - 隐蔽的文件描述符传递
 */

#ifndef ZN_STEALTH_H
#define ZN_STEALTH_H

#include <stdbool.h>
#include <sys/types.h>

/**
 * zn_stealth_init - 初始化所有隐藏性措施
 *
 * 在 zygiskd 启动时尽早调用。
 */
void zn_stealth_init(void);

/**
 * zn_stealth_set_proc_name - 设置伪装进程名
 *
 * 使用 prctl(PR_SET_NAME) 设置进程名，使其在 ps 输出中
 * 显示为无害的系统进程名，而非 "zygiskd"。
 *
 * @param name 要显示的进程名（如 "netd", "logd" 等伪装名）
 */
void zn_stealth_set_proc_name(const char *name);

/**
 * zn_stealth_set_dumpable - 控制 core dump
 *
 * 设置为非 dumpable 防止内存被转储分析。
 *
 * @param dumpable true = 允许 dump, false = 禁止
 */
void zn_stealth_set_dumpable(bool dumpable);

/**
 * zn_stealth_set_ptracer - 设置允许的 ptracer
 *
 * PR_SET_PTRACER 可以限制谁能 attach 到此进程。
 * 设置为特定值可以防止未授权的调试。
 *
 * @param tracer_pid 允许的 tracer PID（0 = 仅祖先，特定值 = 仅该进程）
 */
void zn_stealth_set_ptracer(pid_t tracer_pid);

/**
 * zn_stealth_hide_socket - 隐藏 Unix socket 文件
 *
 * 将 socket 文件放在不显眼的路径下。
 * 使用抽象的 socket 命名空间（不创建文件系统条目）。
 */
int zn_stealth_create_hidden_socket(const char *abstract_name);

#endif /* ZN_STEALTH_H */
