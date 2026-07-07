/**
 * zn_constants.h - ReZygisk Next 常量定义
 *
 * 所有文件路径、Socket 名称等敏感字符串的集中定义。
 * 当 OBFUSCATE_STRINGS=1 时，这些会被预处理器自动加密。
 */

#ifndef ZN_CONSTANTS_H
#define ZN_CONSTANTS_H

/* ====================================================
 * 模块标识
 * ==================================================== */
#define ZN_MODULE_ID        "rezygisk"
#define ZN_MODULE_NAME      "ReZygisk Next"

/* ====================================================
 * 文件路径
 * ==================================================== */
#define ZN_PATH_BASE        "/data/adb/rezygisk"
#define ZN_PATH_MODULES     "/data/adb/modules"
#define ZN_PATH_SELF_MODULE "/data/adb/modules/rezygisk"

/* zygiskd 路径 */
#define ZN_PATH_ZYGISKD_BIN ZN_PATH_SELF_MODULE "/bin/zygiskd"
#define ZN_PATH_ZYGISKD_32  ZN_PATH_ZYGISKD_BIN "32"
#define ZN_PATH_ZYGISKD_64  ZN_PATH_ZYGISKD_BIN "64"

/* Socket 路径 */
#ifdef __LP64__
    #define ZN_SOCKET_NAME  ZN_PATH_BASE "/cp64.sock"
#else
    #define ZN_SOCKET_NAME  ZN_PATH_BASE "/cp32.sock"
#endif

#define ZN_SOCKET_CONTROLLER ZN_PATH_BASE "/init_monitor"

/* ====================================================
 * 库文件路径
 * ==================================================== */
#define ZN_LIB_ZYGISK       "libzygisk.so"
#define ZN_LIB_PTRACE       "libzygisk_ptrace.so"
#define ZN_LIB_PAYLOAD      "libpayload.so"
#define ZN_LIB_LOADER       "libzn_loader.so"

/* ====================================================
 * 版本信息
 * ==================================================== */
#ifndef ZKSU_VERSION
    #define ZN_VERSION_STR  "v2.0.0-dev"
#else
    #define ZN_VERSION_STR  ZKSU_VERSION
#endif

/* ====================================================
 * Zygisk 协议常量
 * ==================================================== */

/* 守护进程动作 */
enum ZnDaemonAction {
    ZN_ACT_ZYGOTE_INJECTED       = 0,
    ZN_ACT_GET_PROCESS_FLAGS     = 1,
    ZN_ACT_GET_INFO              = 2,
    ZN_ACT_READ_MODULES          = 3,
    ZN_ACT_REQUEST_COMPANION     = 4,
    ZN_ACT_GET_MODULE_DIR        = 5,
    ZN_ACT_ZYGOTE_RESTART        = 6,
    ZN_ACT_UPDATE_MOUNT_NS       = 7,
    ZN_ACT_REMOVE_MODULE         = 8,
};

/* 进程标志位 */
#define ZN_FLAG_GRANTED_ROOT      (1u << 0)
#define ZN_FLAG_ON_DENYLIST       (1u << 1)
#define ZN_FLAG_IS_FIRST_STARTED  (1u << 2)
#define ZN_FLAG_IS_MANAGER        (1u << 3)
#define ZN_FLAG_ROOT_IS_KSU       (1u << 28)
#define ZN_FLAG_ROOT_IS_APATCH    (1u << 29)
#define ZN_FLAG_ROOT_IS_MAGISK    (1u << 30)

/* ====================================================
 * 内存注入配置
 * ==================================================== */

/* 注入代码的最大大小 */
#define ZN_INJECT_MAX_SIZE        (64 * 1024)  /* 64KB */

/* mremap 对齐要求 */
#define ZN_MREMAP_ALIGN           4096

/* process_vm_writev 最大单次传输 */
#define ZN_PROC_VM_MAX_CHUNK      (1024 * 1024)  /* 1MB */

/* ====================================================
 * Payload 配置
 * ==================================================== */

/* 加密 machikado 数据的大小 */
#define ZN_MACHIKADO_SIZE         96

/* 加密 mazoku 密钥的大小 */
#define ZN_MAZOKU_SIZE            97

#endif /* ZN_CONSTANTS_H */
