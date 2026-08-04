# LSPosed 框架本地位置与隐藏情况盘点

> 日期：2026-08-04
> 关联：`spec/app-view-hiding-survey.md`（app 视角整体隐藏）
> 实测设备：PGEM10 / Android16 / APatch / ReZygisk v125 + zygisk_lsposed v2.0.2

---

## 一、框架本地位置

| 项 | 值 |
|----|-----|
| **活跃维护版（Vector）** | `F:\_Android\Vector\`（JingMatrix 的 LSPosed 维护版，原名 LSPosed fork） |
| Vector 版本 | `de6e250`（master，2026-08-04；tag `v2.1` = 2026-08-02） |
| 官方停更版（保留参考） | `F:\_Android\LSPosed\`（LSPosed/LSPosed，master 停 2024-01，release 停 v1.9.2） |
| 设备实装 | zygisk_lsposed **v2.0.2 (7668)**（官方 Telegram 渠道 t.me/LSPosed，非 GitHub tag） |
| 拉取方式 | 代理：`git clone https://github.chenc.dev/https://github.com/JingMatrix/Vector` |

> **版本机制**：`verCode = commitCount + 4200`，`verName = 最近 tag`（build.gradle.kts:60-61）。
> 设备 2.0.2 (7668) → commitCount 3468；官方 LSPosed 停更后，**JingMatrix/Vector 是事实上的维护延续**，master 每日更新（v2.1 = 2026-08-02）。

---

## 二、LSPosed 隐藏情况盘点（设备实测）

### ReZygisk 已隐藏的（native 注入层干净）

| 项 | 状态 |
|----|------|
| libzygisk.so | 自卸载，app maps 无残留 |
| LSPosed 模块 native 代码 | memfd「jit-cache」伪装加载（inode 2120，fork 继承，路径隐藏） |
| zygiskd | 进程名伪装 `logd monitor` |

### LSPosed 自身暴露点（按风险排序，ReZygisk 控制不到）

| 风险 | 检测点 | 实测 |
|------|--------|------|
| ★★★★★ | **模块 dex 以 base.apk 文件映射执行** | 被注入 app（com.oplus.battery）maps 出现 `.../com.luckyzyx.luckytool.../base.apk` r-xp，**完整包名可见**，任何扫 maps 的检测秒命中 |
| ★★★★ | `@lspbridge-*`、`@lsp_dex2oat*` 抽象 socket | `/proc/net/unix` 明文可见（shell 域实测可见） |
| ★★★★ | **lspd 进程 cmdline=`lspd`** | 未伪装；cwd/environ/父进程路径均指向模块目录 |
| ★★★ | 内存扫 `org.lsposed.*` / Xposed API | LSPosed framework 混入 ART 堆（maps 层隐藏，但类名可被 ClassLoader 扫描命中） |
| ★★ | 私有 jit-cache memfd（2MB） | 名字伪装成 ART JIT，仅高级检测因体积/私有 flag 起疑 |
| ★★ | lspd 的 cwd/environ/父进程/maps 里 `/data/adb/lspd` | 仅 root/系统级可见（普通 app SELinux 读不到） |

### 关键洞察

- 被注入 app 的最大暴露是 **LuckyTool 模块 dex 的 base.apk 文件映射**（LSPosed 模块加载逻辑：mmap APK 跑 dex），fd 已关但**路径留在 maps**。
- 非注入 app 完全干净（无 base.apk 映射、无 memfd 模块）。
- 注入完成后 app 与 lspd 的 IPC 连接即断，无持久 fd 痕迹（好的一面）。

### 建议方向

1. **ReZygisk 可帮**：hook app 内 `mmap`/`open` 把 APK 映射路径替换为匿名/memfd——需进 app 的 PLT hook 层，工程量大（通用路径隐藏）。
2. **LSPosed 侧控制**：lspd 进程名伪装、`@lspbridge`/`@lsp_dex2oat` socket 改名——需改 LSPosed 源码（本地已有框架可改）。
