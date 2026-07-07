/**
 * payload_entry.c - ReZygisk Next Payload 入口
 *
 * 这是注入到 Zygote 进程的最小化引导代码。
 * 负责以纯 syscall 方式启动 zygiskd 守护进程。
 *
 * 设计参考: Zygisk-Next libpayload.so 的 my_execve + my_wait4
 *
 * 关键特征:
 * - 零 libc 依赖（不链接 libc.so, libdl.so, liblog.so）
 * - 纯内核系统调用
 * - 只使用 execve + wait4 两个 syscall 完成引导链
 * - 不在 /proc/pid/maps 中留下 libc 依赖痕迹
 */

#include <asm/unistd.h>
#include <linux/elf.h>
#include <sys/types.h>

#include "syscall.h"

/* zygiskd 守护进程的默认路径（加密存储，运行时解密） */
static const char *DAEMON_PATH = NULL;  /* 由注入器设置 */

/**
 * payload_entry - Payload 入口函数
 *
 * 由注入器 (libzn_loader.so) 在 Zygote 进程内存中调用。
 *
 * @param daemon_path zygiskd 守护进程的完整路径
 * @param flags       执行标志 (保留)
 * @return 0=成功, <0=错误码
 */
int payload_entry(const char *daemon_path, unsigned int flags) {
    (void)flags;

    DAEMON_PATH = daemon_path;
    if (!DAEMON_PATH) {
        return -1;  /* EINVAL - 无路径 */
    }

    /* 构建 zygiskd 的参数数组：
     * zygiskd daemon [--debug]
     */
    char *argv[] = {
        (char *)DAEMON_PATH,
        "daemon",
        NULL
    };

    /* 环境变量：告知 zygiskd 这是 payload 启动模式 */
    char *envp[] = {
        "PAYLOAD_LAUNCHED=1",
        NULL
    };

    /* 使用纯 syscall 执行 zygiskd */
    long ret = payload_execve(DAEMON_PATH, argv, envp);

    /* execve 成功不会返回。如果返回，说明失败 */
    return (int)ret;  /* 负 errno */
}
