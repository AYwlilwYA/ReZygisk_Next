""" ReZygisk Next 模块打包 — 替代 zip 命令, 支持 Windows """
import zipfile
import os
import sys

def pack(src: str, dst: str) -> None:
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
        for root, dirs, files in os.walk(src):
            for f in files:
                if f == '.DS_Store':
                    continue
                full = os.path.join(root, f)
                rel = os.path.relpath(full, src).replace(os.sep, '/')
                z.write(full, rel)
    print(f'ZIP: {os.path.getsize(dst)} bytes -> {dst}')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'用法: {sys.argv[0]} <模块目录> <输出zip>')
        sys.exit(1)
    pack(sys.argv[1], sys.argv[2])
