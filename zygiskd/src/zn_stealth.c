/**
 * zn_stealth.c — zygiskd 隐藏性增强实现
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "utils.h"
#include "zn_stealth.h"

/* ====================================================
 * 进程名伪装
 * ==================================================== */

void zn_stealth_init(void) {
    /* 1. 设置非 dumpable */
    zn_stealth_set_dumpable(false);

    /* 2. 设置进程名伪装
     *    使用常见的系统进程名以避免引起怀疑
     *    具体伪装名根据 root 实现类型动态调整 */
    const char *fake_names[] = {
        "netd",          /* 网络守护进程 — 常见且无害 */
        "logd",          /* 日志守护进程 */
        "servicemanager", /* 服务管理器 */
        "installd",      /* 包安装守护进程 */
    };

    /* 使用 PID 选择伪装名，避免同一设备上多个 zygiskd 同名 */
    size_t idx = (size_t)getpid() % (sizeof(fake_names) / sizeof(fake_names[0]));
    zn_stealth_set_proc_name(fake_names[idx]);

    /* 3. 设置 ptracer 限制
     *    只允许 init (PID=1) 作为 tracer */
    zn_stealth_set_ptracer(1);

    LOGI("Stealth initialized: name=%s, dumpable=off", fake_names[idx]);
}

void zn_stealth_set_proc_name(const char *name) {
    if (!name) return;

    /* PR_SET_NAME 限制名称为 15 个字符（不含 null） */
    if (prctl(PR_SET_NAME, name, 0, 0, 0) < 0) {
        LOGW("PR_SET_NAME failed for '%s': %s", name, strerror(errno));
    }
}

void zn_stealth_set_dumpable(bool dumpable) {
    if (prctl(PR_SET_DUMPABLE, dumpable ? 1 : 0, 0, 0, 0) < 0) {
        LOGW("PR_SET_DUMPABLE failed: %s", strerror(errno));
    }
}

void zn_stealth_set_ptracer(pid_t tracer_pid) {
    if (prctl(PR_SET_PTRACER, tracer_pid, 0, 0, 0) < 0) {
        /* PR_SET_PTRACER 可能在旧内核上不支持，不致命 */
        LOGI("PR_SET_PTRACER(%d) not supported or failed: %s",
             tracer_pid, strerror(errno));
    }
}

/* ====================================================
 * Socket 隐藏
 * ==================================================== */

/**
 * 创建使用抽象命名空间的 Unix domain socket。
 *
 * 抽象 socket 地址的第一个字节为 '\0'，后面的字节是名称。
 * 这种 socket 不会在文件系统中创建条目，ls 看不到。
 */
int zn_stealth_create_hidden_socket(const char *abstract_name) {
    if (!abstract_name) return -1;

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LOGE("socket failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    /* 抽象命名空间：sun_path[0] = '\0'
     * sun_path 从索引 1 开始存放名称 */
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, abstract_name, sizeof(addr.sun_path) - 2);

    /* 抽象 socket 的长度计算：family + 起始 '\0' + 名称 */
    socklen_t addr_len = (socklen_t)(
        offsetof(struct sockaddr_un, sun_path) + 1 + strlen(abstract_name)
    );

    /* 设置 SO_PASSCRED 以支持凭证传递 */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt));

    if (bind(fd, (struct sockaddr *)&addr, addr_len) < 0) {
        LOGE("bind abstract socket '%s' failed: %s",
             abstract_name, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 8) < 0) {
        LOGE("listen failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    LOGI("Created hidden abstract socket '%s' (fd=%d)", abstract_name, fd);
    return fd;
}
