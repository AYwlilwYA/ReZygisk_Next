/**
 * zn_strings.h - 集中加密字符串存储
 *
 * 所有敏感路径和日志 TAG 在编译时 XOR 加密存入 .rodata.obf 段，
 * 运行时通过 zn_strings_get() 解密获取，用完立即清零。
 *
 * 配合 tools/generate_strings.py 自动生成 zn_strings.c
 */
#ifndef ZN_STRINGS_H
#define ZN_STRINGS_H

/* 字符串索引 — 与 generate_strings.py 中的 STRINGS 字典顺序一致 */
enum zn_string_id {
  /* 路径 */
  ZN_PATH_MODULE_DIR,           /* /data/adb/modules/rezygisk */
  ZN_PATH_LIBZYGISK_64,         /* /data/adb/modules/rezygisk/lib64/libzygisk.so */
  ZN_PATH_LIBZYGISK_32,         /* /data/adb/modules/rezygisk/lib/libzygisk.so */
  ZN_PATH_ZYGISKD,              /* /data/adb/modules/rezygisk/bin/zygiskd */
  ZN_PATH_RUNTIME,              /* /data/adb/rezygisk */
  ZN_PATH_MODULE_PROP,          /* module.prop */
  ZN_PATH_MODULE_PROP_BAK,      /* module.prop.bak */
  ZN_PATH_POST_FS_DATA_D,       /* /data/adb/post-fs-data.d */
  ZN_PATH_REZYGISK_SH,          /* rezygisk.sh */

  /* 日志 TAG */
  ZN_TAG_INJECTOR,              /* zygisk-injector */
  ZN_TAG_MONITOR,               /* zygisk-monitor */
  ZN_TAG_CORE,                  /* zygisk-core */
  ZN_TAG_PTRACE,                /* zygisk-ptrace */
  ZN_TAG_ZINJECT,               /* zygisk-zinject */

  /* 二进制/名称 */
  ZN_NAME_ZYGISKPTRACE64,       /* zygisk-ptrace64 */
  ZN_NAME_ZYGISKPTRACE32,       /* zygisk-ptrace32 */
  ZN_NAME_ZYGISKD,              /* zygiskd */
  ZN_NAME_COMPANION,            /* companion */
  ZN_NAME_MONITOR,              /* monitor */

  /* 库名 */
  ZN_LIB_LIBZYGISK,             /* libzygisk.so */
  ZN_LIB_LIBC,                  /* libc.so */
  ZN_LIB_LIBDL,                 /* libdl.so */
  ZN_LIB_LIBZYGISK_PTRACE,      /* libzygisk_ptrace.so */

  /* Selinux 环境变量 */
  ZN_ENV_PAYLOAD_LAUNCHED,      /* PAYLOAD_LAUNCHED=1 */

  ZN_STRING_COUNT
};

/**
 * zn_strings_init — 在模块入口一次性解密所有字符串
 * 应在日志初始化之前调用
 */
void zn_strings_init(void);

/**
 * zn_strings_get — 获取解密后的字符串指针
 * 返回的指针在下一次 get 调用时可能被覆盖，请立即使用或复制
 */
const char *zn_strings_get(enum zn_string_id id);

/**
 * zn_strings_wipe — 安全清零所有已解密的字符串缓冲区
 * 在不需要字符串后调用（通常在初始化完成后）
 */
void zn_strings_wipe(void);

#endif /* ZN_STRINGS_H */
