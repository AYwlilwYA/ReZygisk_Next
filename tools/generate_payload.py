#!/usr/bin/env python3
"""
generate_payload.py - ReZygisk Next 加密引导数据生成器

功能：
1. 读取 libpayload.so，生成加密的 machikado 引导数据
2. 生成 mazoku 密钥/配置数据
3. 支持不同架构的 payload

使用方法：
  python3 tools/generate_payload.py --arch arm64-v8a --payload build/obj/release/payload/arm64-v8a/stripped/libpayload.so --out module/src/

输出：
  module/src/machikado.<arch> — 加密的引导数据（96 字节）
  module/src/mazoku         — 密钥/配置（97 字节）
"""

import sys
import os
import struct
import hashlib
import argparse


def xor_encrypt(data: bytes, key: bytes) -> bytes:
    """循环 XOR 加密"""
    result = bytearray()
    key_len = len(key)
    for i, b in enumerate(data):
        result.append(b ^ key[i % key_len])
    return bytes(result)


def generate_machikado(payload_path: str, arch: str) -> bytes:
    """
    从 libpayload.so 生成 machikado 引导数据。

    格式（96 字节）：
    - 0x00-0x03: 魔数 (0x4D434844 = "MCHD")
    - 0x04-0x07: 架构标识
    - 0x08-0x0F: payload SHA256（前8字节）
    - 0x10-0x13: payload 大小
    - 0x14-0x5F: 加密的入口偏移/参数（76 字节）

    架构标识：
    - 0x01: arm64-v8a
    - 0x02: armeabi-v7a
    - 0x03: x86
    - 0x04: x86_64
    """

    arch_codes = {
        'arm64-v8a':   0x01,
        'armeabi-v7a': 0x02,
        'x86':         0x03,
        'x86_64':      0x04,
    }

    if arch not in arch_codes:
        raise ValueError(f"Unsupported architecture: {arch}")

    # 读取 payload
    with open(payload_path, 'rb') as f:
        payload = f.read()

    # 计算 SHA256
    payload_hash = hashlib.sha256(payload).digest()

    # 构建 machikado 头部
    magic = b'MCHD'  # 4 bytes
    arch_code = struct.pack('<I', arch_codes[arch])  # 4 bytes
    hash_prefix = payload_hash[:8]  # 8 bytes
    payload_size = struct.pack('<I', len(payload))  # 4 bytes

    header = magic + arch_code + hash_prefix + payload_size  # 20 bytes

    # 加密数据区（76 字节）
    # 生成密钥（基于 hash 的确定性密钥）
    key = hashlib.sha256(payload_hash + arch_code).digest()[:16]

    # 构建数据区
    # 包含 entry_offset, flags, reserved
    data = bytearray(76)

    # entry_offset: 0（相对于 payload 基址）
    struct.pack_into('<I', data, 0, 0)

    # flags: 0
    struct.pack_into('<I', data, 4, 0)

    # 加密数据区
    encrypted_data = xor_encrypt(bytes(data), key)

    # 组合: 头 + 加密数据
    machikado = header + encrypted_data

    # 确保正好 96 字节
    assert len(machikado) == 96, f"machikado must be 96 bytes, got {len(machikado)}"

    return machikado


def generate_mazoku(payload_path: str) -> bytes:
    """
    生成 mazoku 密钥/配置数据。

    格式（97 字节）：
    - 0x00-0x03: 魔数 (0x4D5A4B55 = "MZKU")
    - 0x04-0x13: 加密密钥 (16 字节)
    - 0x14-0x60: 配置数据 (77 字节，加密)
    """

    with open(payload_path, 'rb') as f:
        payload = f.read()

    payload_hash = hashlib.sha256(payload).digest()

    magic = b'MZKU'  # 4 bytes

    # 生成主密钥
    master_key = hashlib.sha256(
        payload_hash + b'ReZygiskNext_v2.0.0'
    ).digest()[:16]

    # 配置数据（加密）
    config = bytearray(77)
    # version (2 bytes)
    struct.pack_into('<H', config, 0, 0x0200)  # v2.0
    # features (2 bytes)
    flags = 0
    flags |= 0x0001  # process_vm_writev 支持
    flags |= 0x0002  # mremap 支持
    flags |= 0x0004  # machikado 引导
    flags |= 0x0008  # 抽象 socket
    struct.pack_into('<H', config, 2, flags)

    # 加密配置
    config_key = hashlib.sha256(master_key + b'config').digest()[:16]
    encrypted_config = xor_encrypt(bytes(config), config_key)

    # 组合
    mazoku = magic + master_key + encrypted_config

    assert len(mazoku) == 97, f"mazoku must be 97 bytes, got {len(mazoku)}"

    return mazoku


def main():
    parser = argparse.ArgumentParser(
        description='ReZygisk Next Machikado/Mazoku Generator'
    )
    parser.add_argument('--arch', required=True,
                        help='Target architecture (arm64-v8a, armeabi-v7a, x86, x86_64)')
    parser.add_argument('--payload', required=True,
                        help='Path to libpayload.so')
    parser.add_argument('--out', required=True,
                        help='Output directory')
    args = parser.parse_args()

    # 验证 payload 文件
    if not os.path.isfile(args.payload):
        print(f"Error: payload not found: {args.payload}")
        sys.exit(1)

    # 创建输出目录
    os.makedirs(args.out, exist_ok=True)

    # 生成 machikado
    print(f"Generating machikado.{args.arch} from {args.payload}")
    machikado_data = generate_machikado(args.payload, args.arch)

    machikado_path = os.path.join(args.out, f'machikado.{args.arch}')
    with open(machikado_path, 'wb') as f:
        f.write(machikado_data)
    print(f"  -> {machikado_path} ({len(machikado_data)} bytes)")

    # 生成 mazoku（只在 arm64 时生成，所有架构共享同一个 mazoku）
    if args.arch == 'arm64-v8a':
        print(f"Generating mazoku...")
        mazoku_data = generate_mazoku(args.payload)

        mazoku_path = os.path.join(args.out, 'mazoku')
        with open(mazoku_path, 'wb') as f:
            f.write(mazoku_data)
        print(f"  -> {mazoku_path} ({len(mazoku_data)} bytes)")

    print("Done!")


if __name__ == '__main__':
    main()
