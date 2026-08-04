# 应用视角隐藏 — 全面梳理

> 日期：2026-08-03
> 状态：梳理完成（只读分析，未改代码）
> 触发：ThePedroo 对 ReZygisk_Next 的回复指向原版 `misc.c#L244`（`parse_maps_safe` 内部），
> 点醒此前所有隐藏工作都集中在 **root 侧（外部视角）**，而真正的对抗战场是
> **app 进程自身视角**——app 看自己的运行时状态时，能看到哪些我们的痕迹。

---

## 1. 架构基线：app 进程内到底发生了什么

**关键事实：libzygisk.so 注入的是 zygote，不是 app。** app 进程是 zygote fork 出来的，
它"继承"了 zygote 里做好的一切，再在 specialize 阶段执行自己的清理逻辑。

app 进程内的完整时间线：

```
1. 继承 zygote 中的一切
   ├─ libzygisk.so 文件映射（高地址 0x100000000+，真实路径）
   ├─ 已加载的 Zygisk 模块 .so（csoloader 文件映射）
   ├─ 被改写的 PLT/GOT（libandroid_runtime.so / libart.so）
   └─ 被改写的 ART 方法表（Zygote native 方法 entrypoint）
2. nativeForkAndSpecialize* 包装器（jni_hooks.h）
   rz_fork_pre → specialize_pre → fork → specialize_post → rz_cleanup
3. fd 消毒（hook.c rz_sanitize_fds）——关闭所有泄漏 fd
4. Denylist app：setns 切到干净 mount namespace（hook.c update_mnt_ns）
5. ART VM 启动 → libart.so 的 pthread_attr_setstacksize hook 触发
   unhook GOT → csoloader_deinit → munmap(libzygisk.so)   ← libzygisk.so 自卸载
6. 模块 .so：csoloader_abandon（只释放内部簿记，不 munmap）← 永久残留
```

**核心结论（先行立住）：**
app 静止状态下 libzygisk.so 自身会被卸载、GOT/ART 表会被还原，
**唯一永久残留的强特征是「模块 .so 的 file-backed 映射」**。

---

## 2. app 进程内实际执行的代码路径

| # | 代码路径 | 做了什么 | app 可见影响 | 风险 |
|---|---------|---------|-------------|------|
| 1 | `jni_hooks.h:8-458` | 包装 nativeForkAndSpecialize 等 20 个重载，pre/post/cleanup | ArtMethod entrypoint 被改写后还原（`hook.c:381,1228`） | 低 |
| 2 | `hook.c:824-929` | fork 前置 SIGCHLD + 记录 allowed fds + 遍历 `/proc/self/fd` 关闭泄漏 fd | `/proc/self/fd` 被清洗 | 低 |
| 3 | `plti.c:235-345` | PLT hook：GOT 改写；RELRO 段 mremap 到高地址 + 原位匿名 RW 替换 | 启动早期 libandroid_runtime.so 的 RELRO/GOT 段在 maps 中变成匿名段、原段在高地址；GOT 指向 libzygisk.so | 中（早期窗口，卸载后还原） |
| 4 | `linker.c:648,751` + `hook.c:1002-1018` + `csoloader.c:95` | csoloader 文件映射加载模块 .so；abandon 只释放簿记不 unmap | **模块 .so 以真实 `/data/adb/modules/...` 路径文件映射，app 全生命周期残留，基址可预测** | **高（Top 1）** |
| 5 | `hook.c:1033-1143` | specialize：隔离服务 UID 反推、查 daemon flags、denylist setns、manager setenv `ZYGISK_ENABLED=1` | denylist 的 mountinfo/ns/mnt 变化；manager 的 environ 多一条 | 中 |
| 6 | `hook.c:302-342` | ART 线程创建触发：unhook + deinit + `munmap(start_addr, block_size)` 自卸载 | libzygisk.so 从 maps 消失；**若时序失败则永久残留** | **高（Top 2）** |
| 7 | `ptrace_clear.c:39-108` | zygote 装 seccomp BPF filter，触发一次 PTRACE_EVENT_SECCOMP 清 ptrace_message | 仅内核 <5.10 生效；现代内核跳过（seccomp filters 可见） | 低 |
| 8 | `entry.c:11` + `zn_strings.c:94` | 解密 24 个敏感字符串 | 早期窗口内存可被特征扫描；unmap 后消失 | 中 |

---

## 3. 现有 app 侧隐藏机制盘点

### 3.1 已覆盖的面

| 面 | 机制 | 文件:行号 | 覆盖度 |
|----|------|-----------|--------|
| `/proc/self/maps` 的 atime 检测 | `parse_maps_safe`：fork + socketpair + SCM_RIGHTS 传 fd，由**子进程**读 maps，atime 落在子进程 proc inode，不碰父进程 | `misc.c:58-262`（调用点 `hook.c:453,558,618`） | 完整 |
| libzygisk.so 自身在 maps 的可见性 | specialize 后 ART 线程触发自 munmap | `hook.c:302-342,1220-1279` | 完整 |
| `/proc/self/fd` 泄漏 | fd 消毒（关闭非 allowed fd） | `hook.c:875-927` | 完整 |
| 模块路径挂载可见性 | Denylist setns 切干净 mount namespace（zygiskd 构造，app setns） | `hook.c:191-225,1122-1142` + `zygiskd/src/utils.c:745-874` | 完整 |
| ptrace_message 残留 | seccomp BPF + PTRACE_EVENT_SECCOMP 触发清理 | `ptrace_clear.c:39-108` | 部分（仅 <5.10） |
| 敏感字符串 | zn_strings XOR 加密到 .rodata.obf | `zn_strings.c` / `obfuscate.h` | 完整（仅覆盖 24 条） |

### 3.2 明显缺失的面

| 面 | 缺失原因 |
|----|---------|
| **文件操作类 hook**（read/stat/fopen/openat/access/getattr） | 全工程无任何此类 hook，无法在 app 内对特定路径运行时屏蔽 |
| **maps 内容过滤** | 只"藏自身"，不隐藏可疑映射段 |
| **TracerPid 屏蔽（app 内）** | 靠 ptracer 用 process_vm_* 从源头不写 TracerPid，但 app 被外部 ptrace 时无保护 |
| **`/proc/self/cmdline`、`/proc/self/exe`（app 内）** | 伪装仅存在于 monitor/daemon 进程 |
| **mountinfo 逐条过滤** | 采用整体切 namespace，无单路径 mount 隐藏 |
| **seccomp 审计对抗** | 除 ptrace_message 一次性 trick 外无其他 |

### 3.3 需单独记录的事实

- **抽象 socket 未启用**：`zn_stealth_create_hidden_socket`（`zygiskd/src/zn_stealth.c`）已定义但**从未被调用**。
  daemon 实际仍用文件系统 socket `/data/adb/rezygisk/cp64.sock`（`loader/src/common/daemon.c:18`）。需进一步确认是否应启用。
- **CSOLOADER_MAKE_LINKER_HOOKS 未定义**（`csoloader/Makefile`）：模块库不会出现在 `dl_iterate_phdr` 输出里——
  对 backtrace 隐蔽是**好消息**；但"maps 有、solist 无"的不一致本身是检测点。
- **`__tls_get_addr` 总是被劫持**（`linker.c:1413`）：libzygisk.so unmap 后模块若使用 TLS 会崩溃——稳定性问题，非检测问题。
- **`hook.c:316` `!should_unmap_zygisk` 分支从未被置 false**：疑似死代码/逻辑不完整，需确认。

---

## 4. 十二类检测面对照表

| # | 检测面 | 我们的现状 | 是否暴露 | 风险 |
|---|--------|-----------|---------|------|
| 1 | maps 扫 libzygisk.so | unload 流程 munmap，静止态无；fork→ART 启动前有窗口 | 窗口期暴露 | 中高 |
| 2 | maps 扫模块 .so | **csoloader 文件映射 + abandon 不 unmap，永久残留** | **是（永久）** | **高** |
| 3 | maps 匿名 RWX | 模块段 mmap RW→mprotect X；PLTI RELRO 替换临时匿名 RW | 部分 | 中 |
| 4 | mountinfo | denylist 主动切干净 namespace，无 root 挂载 | 否 | 低 |
| 5 | TracerPid | ptracer 注入后已 DETACH，app 从不被 trace | 否 | 低 |
| 6 | Seccomp | 旧内核继承额外 filter；现代内核跳过 | 旧内核 | 低-中 |
| 7 | atime 检测 | parse_maps_safe 子进程读 maps 规避 | 已覆盖 | 低 |
| 8 | `/proc/self/fd` | 核心 socket 用完即关；模块 getModuleDir/connectCompanion 会留持久 fd | 模块相关 | 中 |
| 9 | cmdline / 进程树 | monitor/zygiskd 伪装 comm + 覆写 cmdline；**但 exe 符号链接仍指向模块目录** | **部分（exe 泄漏）** | **中高** |
| 10 | 内存特征字符串 | unload 前可扫到；静止态基本无；C 代码大量明文未混淆 | 窗口期 | 中 |
| 11 | getprop | app 内不设置模块 prop；manager 被 setenv `ZYGISK_ENABLED=1` | env（仅 manager） | 低-中 |
| 12 | SELinux | sepolicy.rule 注入属前提，app 不可直接读策略 | 低（系统级扫描） | 低-中 |
| 13 | 系统调用行为差异 | fork/strdup/property_get/FileDescriptorInfo/stacksize 被 hook，app 代码运行前已还原 | 早期窗口 | 低 |
| 14 | PLT/GOT 完整性 | PLTI 写 GOT 后还原；**卸载失败则永久残留篡改** | 失败模式 | 中 |
| 15 | maps/solist 不一致 | 模块 maps 可见但 dl_iterate_phdr 无 → 交叉比对可抓 | 是（模块存在时） | 中高 |

---

## 5. Top 暴露点（按风险排序）

1. **模块 .so 文件映射永久残留**（`linker.c:648,659` + `csoloader.c:95` + `hook.c:1002-1018`）
   - 只要设备装了任意 Zygisk 模块，**每个** app 的 maps 都出现 `/data/adb/modules/<mod>/zygisk/<abi>.so`。
   - VMA path 与命名空间无关，DenyList 也挡不住。所有 Zygisk 检测工具的核心特征。
2. **libzygisk.so 在 fork → ART unload 窗口期内的映射**（`remote_csoloader.c:796-837` + `hook.c:302-342`）
   - 任何早期 native 库（JNI_OnLoad / ctor）都能扫到高地址映射 + 明文特征 + 被 hook 的 GOT。
   - 卸载依赖时序，失败则永久残留。
3. **兄弟进程 exe 路径泄漏**（comm 伪装 `monitor.c:998-1013`，但 exe 无法伪装）
   - `/proc/<pid>/exe` 仍指向 `/data/adb/modules/rezygisk/bin/zygisk-ptrace64` 与 `zygiskd64`。
4. **模块 "maps 有、solist 无" 的不一致 + 模块 fd 残留**（`daemon.c:304,323`）
   - 成熟检测交叉比对 maps 与 dl_iterate_phdr，或扫 fd 符号链接。
5. **卸载失败模式的 GOT/ART 永久篡改 + `ZYGISK_ENABLED=1` env**（`hook.c:1349,1220-1239,1106`）

---

## 6. 下一步加固方向（供决策，未实施）

| 优先级 | 方向 | 思路 | 风险/代价 |
|--------|------|------|-----------|
| P0 | 消除模块 .so 残留 | DenyList 路径对模块段 munmap + 记录 dev/inode 供模块按需重载；或 MADV_DONTNEED + 权限收敛 | 模块在 app 内失效，需与 Zygisk API 语义权衡；改动大 |
| P1 | 缩短 libzygisk.so 暴露窗口 | 提前触发自卸载（不依赖 ART 线程）；在注入侧就把基址/特征最小化 | 注入侧改动 |
| P1 | 兄弟进程 exe 伪装 | monitor/zygiskd 用 memfd 或临时目录起进程，使 exe 不指向模块目录 | 启动脚本改动，zygiskd 需自包含 |
| P2 | maps/solist 一致性 | 让模块进入 dl_iterate_phdr（定义 CSOLOADER_MAKE_LINKER_HOOKS）换取一致，或进一步隐藏 maps | 需权衡隐蔽 vs backtrace 兼容 |
| P2 | 启用抽象 socket | 调用 `zn_stealth_create_hidden_socket`，daemon 不再用文件系统 socket | 低风险，先验证调用点 |
| P2 | ZYGISK_ENABLED env | 移除或混淆 manager 标识 | 低风险 |

---

## 7. 结论

- 现状：app 静止态下**自卸 libzygisk.so + 还原 GOT/ART + 切干净 namespace + fd 消毒**已做得很干净，
  **唯一系统性漏网是「模块 .so 永久残留」**。
- 之前工作的着重点：全部在 root 侧（注入不留痕、进程名伪装、字符串静态混淆、TracerPid），
  忽略的正是 app 进程自身运行时的可见性。
- ThePedroo 指向 `parse_maps_safe`（`misc.c#L244` 附近）的潜台词：最难的 app 进程内部隐藏，原版只做了
  atime 这一个点，我们增强隐藏时却连这个视角都没系统覆盖过。

> 待确认事项汇总（见 3.3）：抽象 socket 未启用、CSOLOADER_MAKE_LINKER_HOOKS 未定义、
> `__tls_get_addr` 劫持的稳定性风险、`hook.c:316` 疑似死代码。

---

## 8. 修复后复查（2026-08-03，memfd 生效后 app 视角剩余检测点）

> 方法：真机实测（PDEM30/Android15/APatch）+ 代码对照。检测软件 = app 自身进程视角。

**决定性环境事实**：本机 `/proc` 挂 `hidepid=2,gid=3009(readproc)`，普通 app 不在 readproc 组，
**无法枚举/读取其他进程的 /proc 条目** —— 跨进程检测点几乎全部不可达，检测面收敛到
app 自身进程 + 全局可读文件。

### 剩余检测点清单

| # | 检测点 | 风险 | 状态 | 说明 / 修法 |
|---|--------|------|------|-------------|
| 1 | **`/proc/net/unix` socket 路径泄漏** | **高→已修复** | 2026-08-03 已修 | daemon socket 全部改为**抽象 socket**（复用 `zn_stealth_create_hidden_socket`，`zygiskd/src/zygiskd.c`）：`@resmon_agent`(64)、`@engine_state_ctl`(32)、`@criticallog_evt`(monitor)，名字伪装成本机 OPPO 服务族，libzygisk 端 `daemon.c` 同步改连。真机验证：`/proc/net/unix` 不再出现 `/data/adb/rezygisk/...` 路径，模块正常。剩余：抽象名本身在 `/proc/net/unix` 可见（不暴露路径），且未来固件若新增同名服务会 EADDRINUSE（当前无冲突） |
| 2 | **私有 `r-xp /memfd:jit-cache` 映射** | **高→未达成** | 尝试 MAP_SHARED 失败回退 | 模块段改 MAP_SHARED 后 LSPosed 打包模块 GOT/重定位在共享页异常，zygote 崩溃循环（SIGSEGV/SIGILL），已回退（`linker.c` 无改动）。替代方案待评估：重定位结果 pwrite 回 memfd 后再 MAP_SHARED（改动大）。当前 `r-xp`/`r--p`/`rw-p` 仍与 ART `r--s`/`r-xs`/`rw-s` 可区分 |
| 3 | zygote maps `libzygisk.so` 真实路径 | 中 | 对 root 检测漏网 | app 读不到（hidepid+ptrace）；root 级 su helper 能读。窗口期，根除需初始注入也走 memfd |
| 4 | `/dev/zero (deleted)` 映射 | 中 | 模块相关 | LSPosed 加载链产物（rezygisk 源码无此逻辑），无法直接控制 |
| 5 | `/data/local/tmp` developer 遗留物 | 中 | 设备卫生 | `ReZygisk_Next-v2.0.0-debug.zip`、`libzygisk.so`、`lspd_cfg.tar` 0666 可被 app 读，建议清理 |
| 6 | mountinfo / mount ns | 低 | 已覆盖 | `kernel_umount` 全局卸载，注入 app mountinfo 干净 |
| 7 | app 自身 proc（status/environ/fd/cmdline） | 低 | 已覆盖 | TracerPid=0、无 ZYGISK_ENABLED、fd 干净 |
| 8 | 文件系统可见性 | 低 | 已覆盖 | `/data/adb`（0700）app 不可达 |
| 9 | 内存字符串扫描（模块内容） | 中/低 | 模块相关 | LSPosed Java 框架 `org.lsposed.*` 元数据，框架无法隐藏 |
| 10 | PackageManager | 低 | 已覆盖 | 模块无可见包 |

### 结论

memfd 修复（模块库路径从 app maps 消失）确实生效，app 自身 maps/mountinfo/environ/fd 均干净。
**`/proc/net/unix` 的 socket 路径泄漏是当前最大且完全未覆盖的 app 级检测点**，
其次是与 ART jit-cache 可区分的私有 memfd 映射 flag。
