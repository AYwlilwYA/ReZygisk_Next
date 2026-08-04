#!/usr/bin/env python3
"""
obfuscate_strings.py - ReZygisk Next 字符串预处理器

在编译前对源文件中的 OBF_STR("...") 和 OBF_FMT("...") 标记进行加密。

工作原理：
1. 扫描源文件，找到所有 OBF_STR("string") 和 OBF_FMT("string")
2. 使用随机密钥对字符串进行 XOR 加密
3. 替换为运行时解密代码
4. 输出处理后的源文件

使用方法：
  python3 tools/obfuscate_strings.py <input.c> <output.c>
  python3 tools/obfuscate_strings.py --key 0x5A <input.c> <output.c>

注意：此工具设计为可选。默认情况下使用明文字符串。
启用方式：在 common.mk 中设置 OBFUSCATE_STRINGS=1
"""

import sys
import re
import os
import random
import struct


def xor_encrypt(text: str, key: int) -> bytes:
    """使用 XOR 加密字符串"""
    result = bytearray()
    for i, ch in enumerate(text.encode('utf-8') + b'\x00'):
        result.append(ch ^ ((key + i) & 0xFF))
    return bytes(result)


def format_hex_array(data: bytes) -> str:
    """格式化字节数组为 C 语言十六进制数组"""
    return ', '.join(f'0x{b:02x}' for b in data)


def process_source(content: str, key: int) -> str:
    """处理源文件内容"""

    # 匹配 OBF_STR("...")
    def replace_obf_str(match):
        text = match.group(1)
        encrypted = xor_encrypt(text, key)
        hex_arr = format_hex_array(encrypted)
        return (
            f'({{ static const char __obf[] __attribute__((section(".rodata.obf")))'
            f' = {{ {hex_arr} }}; '
            f'static char __dec[sizeof(__obf)]; '
            f'for(size_t i=0;i<sizeof(__obf);i++)'
            f'__dec[i]=__obf[i]^((char)(0x{key:02x}+i)); '
            f'__dec; }})'
        )

    content = re.sub(r'OBF_STR\("([^"]*)"\)', replace_obf_str, content)

    # 匹配 OBF_FMT("...")
    def replace_obf_fmt(match):
        text = match.group(1)
        encrypted = xor_encrypt(text, key)
        hex_arr = format_hex_array(encrypted)
        return (
            f'({{ static const char __obf[] __attribute__((section(".rodata.obf")))'
            f' = {{ {hex_arr} }}; '
            f'static char __dec[sizeof(__obf)]; '
            f'for(size_t i=0;i<sizeof(__obf);i++)'
            f'__dec[i]=__obf[i]^((char)(0x{key:02x}+i)); '
            f'__dec; }})'
        )

    content = re.sub(r'OBF_FMT\("([^"]*)"\)', replace_obf_fmt, content)

    return content


def main():
    key = None
    args = sys.argv[1:]

    if '--key' in args:
        idx = args.index('--key')
        key_str = args[idx + 1]
        if key_str.startswith('0x'):
            key = int(key_str, 16)
        else:
            key = int(key_str)
        args = args[:idx] + args[idx + 2:]

    if len(args) < 2:
        print(f"Usage: {sys.argv[0]} [--key KEY] <input.c> <output.c>")
        sys.exit(1)

    input_file = args[0]
    output_file = args[1]

    # 如果没有指定密钥，生成随机密钥
    if key is None:
        key = random.randint(1, 255)
        print(f"[obfuscate] Using random key: 0x{key:02x}")

    # 读取源文件
    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read()

    # 处理
    processed = process_source(content, key)

    # 写入预处理后的文件
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(f'/* Auto-obfuscated with key=0x{key:02x} */\n')
        f.write(processed)

    print(f"[obfuscate] Processed {input_file} -> {output_file}")


if __name__ == '__main__':
    main()
