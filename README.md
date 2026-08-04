# ReZygisk Next

ReZygisk 的 Zygisk 独立实现，提供增强的反检测能力。支持 **Magisk**、**KernelSU** 和 **APatch**。

以纯 C 全面重写，内置自定义 linker，不依赖系统 linker 加载，从根本上绕过 linker 级检测。

## 特性

- **纯 C 重写** — 更轻量、更快的二进制文件，代码逻辑清晰可审计
- **自定义 linker** — 基于 CSOLoader，在正常流程中完全绕过系统 linker
- **多级反检测** — 进程名伪装、抽象 socket、加密通信协议、MADV_FREE 内存清理
- **Payload 纯 syscall 引导** — 零 libc 依赖的最小化引导代码，绕过用户态 hook
- **process_vm_writev 注入** — 避免使用 ptrace，降低被检测风险
- **mremap 隐藏加载** — 不创建新 VMA 条目，隐蔽加载动态库
- **字符串混淆** — 编译时/运行时字符串加密
- **OLLVM 混淆支持** — 可选的控制流平坦化、指令替换等混淆

## 支持的 Root 方案

| Root 方案 | 最低版本 |
|-----------|---------|
| Magisk | 26402 |
| KernelSU | 10940 |
| APatch | 10655 |

## 兼容性

- **Android**: 7.1+ (API 25+)
- **架构**: arm64-v8a, armeabi-v7a, x86, x86_64
- **Zygisk API**: 完全兼容 Zygisk API v4 模块

## 安装

1. 从 [Releases](https://github.com/AYwlilwYA/ReZygisk_Next/releases) 下载对应版本
   - `release` — 日常使用，优化过的二进制文件，无调试日志
   - `debug` — 仅调试用，包含详细日志
2. 在 Root 管理器（Magisk/KernelSU/APatch）的模块页面刷入 zip
3. **Magisk 用户**：请在设置中关闭内置 Zygisk，否则会冲突
4. 重启设备

安装后可在模块描述中查看运行状态，正常应显示 `[Monitor: ✅, ReZygisk 64-bit: ✅, ReZygisk 32-bit: ✅]`。

## 构建

### 依赖

- Android NDK (r27+)
- GNU Make
- Python 3

### 构建命令

```sh
# Debug 构建
make debug

# Release 构建
make release

# OLLVM 混淆构建 (需要 OLLVM/Hikari 工具链)
make OBFUSCATE=1 BUILD_TYPE=release
```

输出文件位于 `build/out/` 目录。

## 许可

ReZygisk Next 基于 [AGPL 3.0](./LICENSE) 许可。
