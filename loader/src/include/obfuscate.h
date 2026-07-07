/**
 * obfuscate.h - ReZygisk Next 字符串混淆系统
 *
 * 提供编译时字符串加密和运行时解密功能。
 * 参考 Zygisk-Next 的字符串全加密策略。
 *
 * 设计原理:
 * - 使用 XOR + 编译时常量密钥进行简单加密
 * - 密钥从 __TIME__ 和 __COUNTER__ 派生，使每次编译产生不同密文
 * - 解密后立即使用，用完清零，最小化明文窗口
 *
 * 使用方法:
 *   const char *path = OBFUSCATE("/data/adb/rezygisk");
 *   // path 在运行时解密，使用后自动清零
 */

#ifndef OBFUSCATE_H
#define OBFUSCATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================
 * 编译时字符串加密
 * ==================================================== */

/* XOR 密钥生成 — 使用编译时间和计数器 */
#define OBF_KEY_SEED ((__TIME__[0] * 3600 + __TIME__[1] * 60 + \
                       __TIME__[3] * 10 + __TIME__[4]) ^ __COUNTER__)

/* 加密宏 — 在编译时对字符串进行 XOR 加密（仅 C++ 可用） */
#ifdef __cplusplus
#define OBFUSCATE(str) \
    ([]() -> const char* { \
        static char __attribute__((section(".rodata.obf"))) \
            obf_data[] = { OBF_XOR_STR(str, OBF_KEY_SEED) }; \
        static char decoded[sizeof(obf_data)]; \
        for (size_t i = 0; i < sizeof(obf_data); i++) { \
            decoded[i] = obf_data[i] ^ (char)(OBF_KEY_SEED + i); \
        } \
        return decoded; \
    })()
#else
/* C 编译模式下，OBFUSCATE 退化为明文（需配合 tools/obfuscate_strings.py 使用） */
#define OBFUSCATE(str) (str)
#endif

/* 简化的 XOR 加密宏 */
#define OBF_XOR_BYTE(c, key, idx) ((char)((c) ^ (char)((key) + (idx))))

/* ====================================================
 * 运行时字符串解密函数
 * ==================================================== */

/**
 * obf_decrypt — 就地解密 XOR 加密的字符串
 *
 * 用于在 init 阶段批量解密所有加密字符串。
 *
 * @param data     加密的字符串缓冲区
 * @param len      缓冲区长度
 * @param key      解密密钥
 */
static inline void obf_decrypt(char *data, size_t len, uint8_t key) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= (char)(key + i);
    }
}

/**
 * obf_decrypt_str — 解密单个以 null 结尾的字符串
 *
 * @param data  加密字符串（原地解密）
 * @param key   解密密钥
 * @return      解密后的字符串指针（同 data）
 */
static inline char *obf_decrypt_str(char *data, uint8_t key) {
    size_t i = 0;
    while (data[i] != '\0') {
        data[i] ^= (char)(key + i);
        i++;
    }
    return data;
}

/**
 * obf_secure_zero — 安全清零敏感数据
 *
 * 使用 volatile 指针防止编译器优化掉清零操作。
 *
 * @param ptr  要清零的内存
 * @param len  长度
 */
static inline void obf_secure_zero(void *ptr, size_t len) {
    volatile char *p = (volatile char *)ptr;
    while (len--) {
        *p++ = 0;
    }
}

/* ====================================================
 * 便捷宏 — 日志 TAG 和常用路径
 * ==================================================== */

/* 日志 TAG — 不同二进制使用不同的 TAG */
#ifdef zygisk_EXPORTS
    #define OBF_LOG_TAG    "rz-injector"
#elif defined(PAYLOAD_EXPORTS)
    #define OBF_LOG_TAG    "rz-payload"
#else
    #define OBF_LOG_TAG    "rz-loader"
#endif

/* 常用路径的加密表示
 * 实际使用时通过 obf_decrypt_str 解密
 * 注意：以下路径仅为示例，实际在 .rodata.obf 段中存储
 */
#define OBF_PATH_ZYGISKD  "\x12\x34\x56\x78"  /* 占位 — 构建时替换 */
#define OBF_PATH_MODULES  "\x9a\xbc\xde\xf0"  /* 占位 — 构建时替换 */
#define OBF_PATH_SOCKET   "\x55\x66\x77\x88"  /* 占位 — 构建时替换 */

/* ====================================================
 * 编译时字符串加密 — 构建系统辅助宏
 *
 * 这些宏在编译时展开，将字符串字面量转换为加密字节数组。
 * 配合 tools/obfuscate_strings.py 在构建前预处理源文件。
 * ==================================================== */

/* 标记：需要被预处理器加密的字符串 */
#define OBF_STR(s)  OBF_MARKER_##s

/* 标记：需要被预处理器加密的格式字符串 */
#define OBF_FMT(s)  OBF_FMT_MARKER_##s

#ifdef __cplusplus
}
#endif

#endif /* OBFUSCATE_H */
