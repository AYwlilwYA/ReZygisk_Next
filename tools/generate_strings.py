#!/usr/bin/env python3
"""
generate_strings.py — 生成 zn_strings.c

根据内嵌字符串表，生成 XOR 加密的字符串存储和运行时解密代码。
每个字符串使用独立随机密钥，存入 .rodata.obf 段。
"""

import os
import random
import sys
from pathlib import Path

# 所有敏感字符串 — 顺序必须与 zn_strings.h 的 enum zn_string_id 一致
STRINGS = [
    # ── 路径 ──
    "/data/adb/modules/rezygisk",                              # ZN_PATH_MODULE_DIR
    "/data/adb/modules/rezygisk/lib64/libzygisk.so",           # ZN_PATH_LIBZYGISK_64
    "/data/adb/modules/rezygisk/lib/libzygisk.so",             # ZN_PATH_LIBZYGISK_32
    "/data/adb/modules/rezygisk/bin/zygiskd",                  # ZN_PATH_ZYGISKD
    "/data/adb/rezygisk",                                      # ZN_PATH_RUNTIME
    "module.prop",                                              # ZN_PATH_MODULE_PROP
    "module.prop.bak",                                          # ZN_PATH_MODULE_PROP_BAK
    "/data/adb/post-fs-data.d",                                 # ZN_PATH_POST_FS_DATA_D
    "rezygisk.sh",                                              # ZN_PATH_REZYGISK_SH

    # ── 日志 TAG ──
    "zygisk-injector",     # ZN_TAG_INJECTOR
    "zygisk-monitor",      # ZN_TAG_MONITOR
    "zygisk-core",         # ZN_TAG_CORE
    "zygisk-ptrace",       # ZN_TAG_PTRACE
    "zygisk-zinject",      # ZN_TAG_ZINJECT

    # ── 二进制/名称 ──
    "zygisk-ptrace64",     # ZN_NAME_ZYGISKPTRACE64
    "zygisk-ptrace32",     # ZN_NAME_ZYGISKPTRACE32
    "zygiskd",             # ZN_NAME_ZYGISKD
    "companion",           # ZN_NAME_COMPANION
    "monitor",             # ZN_NAME_MONITOR

    # ── 库名 ──
    "libzygisk.so",            # ZN_LIB_LIBZYGISK
    "libc.so",                 # ZN_LIB_LIBC
    "libdl.so",                # ZN_LIB_LIBDL
    "libzygisk_ptrace.so",     # ZN_LIB_LIBZYGISK_PTRACE

    # ── 环境变量 ──
    "PAYLOAD_LAUNCHED=1",  # ZN_ENV_PAYLOAD_LAUNCHED
]

assert len(STRINGS) == 24, f"Expected 24 strings, got {len(STRINGS)}"


def xor_encrypt(text: str, key: int) -> list[int]:
    """XOR 加密字符串（含结尾 \\0）"""
    result = []
    for i, ch in enumerate(text.encode('utf-8') + b'\x00'):
        result.append(ch ^ ((key + i) & 0xFF))
    return result


def generate_c(output_path: str) -> None:
    """生成 zn_strings.c"""
    keys = [random.randint(1, 255) for _ in STRINGS]
    encrypted = [xor_encrypt(s, k) for s, k in zip(STRINGS, keys)]

    lines = []
    lines.append('/* 自动生成 — 请勿手动编辑 */')
    lines.append('/* 运行 tools/generate_strings.py 重新生成 */')
    lines.append('')
    lines.append('#include "zn_strings.h"')
    lines.append('#include <string.h>')
    lines.append('')
    lines.append('/* XOR 解密密钥（每个字符串独立随机） */')
    lines.append(f'static const uint8_t zn_keys[{len(keys)}] = {{')
    lines.append('    ' + ', '.join(f'0x{k:02x}' for k in keys))
    lines.append('};')
    lines.append('')
    lines.append('/* 加密字符串数据 — .rodata.obf 段 */')
    for i, (arr, s) in enumerate(zip(encrypted, STRINGS)):
        hex_str = ', '.join(f'0x{b:02x}' for b in arr)
        lines.append(f'__attribute__((section(".rodata.obf")))')
        lines.append(f'static const uint8_t zn_enc_{i}[] = {{ {hex_str} }};')
    lines.append('')
    lines.append('/* 加密数据指针表 */')
    lines.append(f'static const struct {{')
    lines.append(f'    const uint8_t *data;')
    lines.append(f'    size_t len;')
    lines.append(f'}} zn_table[{len(STRINGS)}] = {{')
    for i, arr in enumerate(encrypted):
        lines.append(f'    {{ zn_enc_{i}, {len(arr)} }},')
    lines.append('};')
    lines.append('')
    lines.append('/* 解密缓冲区 — 延迟分配 */')
    lines.append(f'static char zn_buf[{len(STRINGS)}][256];')
    lines.append('static int zn_inited = 0;')
    lines.append('')
    lines.append('void zn_strings_init(void) {')
    lines.append('    if (zn_inited) return;')
    lines.append('    for (int i = 0; i < ZN_STRING_COUNT; i++) {')
    lines.append('        uint8_t key = zn_keys[i];')
    lines.append('        size_t n = zn_table[i].len;')
    lines.append('        for (size_t j = 0; j < n; j++) {')
    lines.append('            zn_buf[i][j] = (char)(zn_table[i].data[j] ^ (uint8_t)(key + j));')
    lines.append('        }')
    lines.append('    }')
    lines.append('    zn_inited = 1;')
    lines.append('}')
    lines.append('')
    lines.append('const char *zn_strings_get(enum zn_string_id id) {')
    lines.append('    if (id < 0 || id >= ZN_STRING_COUNT) return NULL;')
    lines.append('    if (!zn_inited) zn_strings_init();')
    lines.append('    return zn_buf[id];')
    lines.append('}')
    lines.append('')
    lines.append('void zn_strings_wipe(void) {')
    lines.append('    for (int i = 0; i < ZN_STRING_COUNT; i++) {')
    lines.append('        volatile char *p = zn_buf[i];')
    lines.append('        size_t n = zn_table[i].len;')
    lines.append('        while (n--) *p++ = 0;')
    lines.append('    }')
    lines.append('    zn_inited = 0;')
    lines.append('}')

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    print(f'[generate_strings] Generated {output_path} ({len(STRINGS)} strings, {sum(len(e) for e in encrypted)} encrypted bytes)')


if __name__ == '__main__':
    dest = sys.argv[1] if len(sys.argv) > 1 else 'loader/src/common/zn_strings.c'
    generate_c(dest)
