/**
 * zn_protocol.h — ReZygisk Next 内部加密通信协议
 *
 * 简单的 XOR 混淆协议，用于 zygiskd 和 libzygisk.so 之间的通信。
 * 不是真正的加密，但可以防止明文通信被直接读取。
 */

#ifndef ZN_PROTOCOL_H
#define ZN_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 协议魔数 — 用于识别 ReZygisk Next 数据包
 */
#define ZN_PROTO_MAGIC  0x5A4E525A  /* "ZRNZ" in little-endian */

/**
 * 协议版本
 */
#define ZN_PROTO_VERSION  2

/**
 * XOR 密钥种子
 * 注意：生产环境中应使用运行时生成的随机密钥
 */
#define ZN_PROTO_XOR_KEY  0xAD

/**
 * 消息头
 */
struct zn_msg_header {
    uint32_t magic;      /* ZN_PROTO_MAGIC */
    uint32_t version;    /* ZN_PROTO_VERSION */
    uint32_t type;       /* 消息类型 */
    uint32_t length;     /* 载荷长度 */
    uint32_t checksum;   /* 简单校验和 (header + payload XOR) */
};

/**
 * 简单 XOR 混淆
 */
static inline void zn_proto_xor(void *data, size_t len, uint8_t key) {
    uint8_t *p = (uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        p[i] ^= (uint8_t)(key + (uint8_t)i);
    }
}

/**
 * 计算校验和
 */
static inline uint32_t zn_proto_checksum(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum = (sum << 1) ^ p[i];
    }
    return sum;
}

/**
 * 构建加密消息包
 */
static inline void zn_proto_pack(void *buf, size_t buf_size,
                                  uint32_t msg_type,
                                  const void *payload, size_t payload_len) {
    if (buf_size < sizeof(struct zn_msg_header) + payload_len) return;

    struct zn_msg_header *hdr = (struct zn_msg_header *)buf;

    hdr->magic   = ZN_PROTO_MAGIC;
    hdr->version = ZN_PROTO_VERSION;
    hdr->type    = msg_type;
    hdr->length  = (uint32_t)payload_len;

    /* 复制并混淆载荷 */
    if (payload && payload_len > 0) {
        uint8_t *body = (uint8_t *)(hdr + 1);
        memcpy(body, payload, payload_len);
        zn_proto_xor(body, payload_len, ZN_PROTO_XOR_KEY);
    }

    /* 计算校验和（先清零 checksum 字段） */
    hdr->checksum = 0;
    hdr->checksum = zn_proto_checksum(hdr,
                                       sizeof(struct zn_msg_header) + payload_len);
}

/**
 * 解包并验证消息
 *
 * @return 载荷长度，0 表示验证失败
 */
static inline size_t zn_proto_unpack(const void *buf, size_t buf_size,
                                      uint32_t *out_type,
                                      void *out_payload, size_t out_size) {
    if (buf_size < sizeof(struct zn_msg_header)) return 0;

    const struct zn_msg_header *hdr = (const struct zn_msg_header *)buf;

    /* 验证魔数 */
    if (hdr->magic != ZN_PROTO_MAGIC) return 0;

    /* 验证版本 */
    if (hdr->version != ZN_PROTO_VERSION) return 0;

    /* 验证长度 */
    size_t total_len = sizeof(struct zn_msg_header) + hdr->length;
    if (total_len > buf_size) return 0;
    if (hdr->length > out_size) return 0;

    /* 验证校验和 */
    uint32_t computed = zn_proto_checksum(buf, total_len);
    if (computed != hdr->checksum) return 0;

    /* 解混淆载荷 */
    if (hdr->length > 0) {
        const uint8_t *body = (const uint8_t *)(hdr + 1);
        memcpy(out_payload, body, hdr->length);
        zn_proto_xor(out_payload, hdr->length, ZN_PROTO_XOR_KEY);
    }

    if (out_type) *out_type = hdr->type;
    return hdr->length;
}

#ifdef __cplusplus
}
#endif

#endif /* ZN_PROTOCOL_H */
