# zygisk-core 打开 mnt ns fd 的 SELinux EACCES（QQ LSPosed 挂载失败）

> 日期：2026-08-04
> 实测设备：PJZ110（一加 13）/ Android 16 / APatch（apd 11219）/ ReZygisk v124-ddd40de-release
> 关联：`spec/zygiskd-startup-fix.md`（monitor 启动修复）、`spec/module-memfd-fix.md`（memfd 伪装加载）
> 现象：QQ（com.tencent.mobileqq）进程内 wxhook（LSPosed 模块）挂载失败

---

## 一、现象与定位

用户在 PJZ110 上验证 ReZygisk Next 运行状态时发现：**QQ 进程内 wxhook（com.butler.wxhook，LSPosed 模块）挂载失败**，而 mark.via / 微信内模块正常注入。

比对 QQ 与 mark.via 的 fork 注入序列（均走 zygisk_lsposed companion 请求）：
- QQ：fork 后 **64ms 报错**，LSPosed 框架未初始化
- mark.via：**338ms 成功注入**（FCK_VIP 模块日志确认）

### 决定性日志（zygiskd verbose）

```
19:14:20.535  pid 31067 (QQ)   E/zygisk-core64  Failed to open mount namespace [/proc/2025/fd/14] failed with 13: Permission denied
19:14:21.047  pid 31334 (QQ:MSF) E/zygisk-core64 Failed to open mount namespace [/proc/2025/fd/14] failed with 13: Permission denied
```

- `pid 2025` = zygiskd64 **伪装进程**（伪装名 `logd`）
- `fd 14` = zygiskd 持有的 mount namespace fd
- **`errno 13 (EACCES)`** = SELinux 拒绝

日志位置：`/data/adb/lspd/log/verbose_*.log`（lspd 转储了 zygiskd 输出）。

## 二、根因（源码 + 设备双重验证）

### 注入链三段式（namespace fd 传递）

```
loader(app 进程 libzygisk)                zygiskd(守护进程)
  hook.c:191 update_mnt_ns  ── 传 pid/state ──▶ zygiskd.c:588 UpdateMountNS
                                                  └─ utils.c:754 save_mns_fd（fork 子进程建缓存）
  hook.c:201 open(/proc/<zygiskd_pid>/fd/N) ◀── 返回 fd 号码(uint32_t, 非 SCM_RIGHTS)
    └─ setns(fd, CLONE_NEWNS)
```

关键点：**zygiskd 返回的是 fd 号码而非 fd 本体**，loader 必须自行
`open("/proc/<zygiskd_pid>/fd/<N>")`（`loader/src/injector/hook.c:201`）才能 setns。
此 open 的 scontext = app 进程（pre-specialize 时 `u:r:zygote:s0`），
tcontext = zygiskd 进程域（伪装 logd，实际 `magisk` 域）。

### sepolicy.rule 缺口（module/src/sepolicy.rule）

```text
allow zygote su dir search
allow zygote su {lnk_file file} read
allow zygote ksu dir search
allow zygote ksu {lnk_file file} read
# ...magisk 域只有一行：
allow zygote magisk lnk_file read        ← 缺 dir search + file read
```

`su`/`ksu` 域都有 `dir search`（遍历 `/proc/<pid>/fd` 目录）+ `{lnk_file file} read`，
而 `magisk` 域只补了 `lnk_file read`。**缺 `dir search` 导致 open `/proc/<zygiskd>/fd/N`
在目录遍历阶段即被拒（EACCES）。**

### 为什么 QQ 失败、mark.via 成功

| app | ReZygisk 分支 | setns 时机 | 结果 |
|-----|--------------|-----------|------|
| **QQ** | denylist → 分支 3（`hook.c:1125`） | **modules pre 之前**真实 setns 清 mount | fd 获取失败 → 注入链中止 → LSPosed 框架未初始化 |
| **mark.via** | 非 denylist → 分支 4（`hook.c:1142`，LSPosed 置 DO_REVERT_UNMOUNT） | modules pre 之后，复用缓存 | EACCES 后有回退，仍注入成功 |

注：日志显示**所有 app fork 都报该 EACCES**（含微信 13495），但仅 denylist 分支
（需真实 setns）是致命的；非 denylist 走回退，LSPosed 仍能初始化。
这是「普遍报错但多数 app 正常」的原因。

## 三、验证证据链

1. **verbose 日志**：`Failed to open mount namespace [/proc/2025/fd/14] ... 13`（大量 pid）
2. **/proc/2025**：`cat /proc/2025/comm` = `logd`（zygiskd64 伪装进程）
3. **抽象 socket**：`@resmon_agent`/`@engine_state_ctl`/`@criticallog_evt` 全在线（zygiskd 正常）
4. **state.json**：root=APatch, memfd=ok, zygiskd 64/32 均 state:1（注入链整体正常）
5. **源码**：`hook.c:201` open 路径、`sepolicy.rule` magisk 缺规则
6. **dmesg**：未抓到 QQ fork 时刻的 AVC（环形缓冲已冲刷），但 verbose 日志（ReZygisk 自记）已坐实 EACCES

## 四、修复方案

`module/src/sepolicy.rule` 将 magisk 域补齐为与 su/ksu 同等：

```text
allow zygote magisk dir search
allow zygote magisk {lnk_file file} read
```

即把第 18 行 `allow zygote magisk lnk_file read` 扩为上述两条。

修复后：QQ 的 denylist setns 能取到 fd → 注入链完整 → LSPosed 框架在 QQ 初始化
→ wxhook 可加载。

## 五、附带发现（wxhook 侧）

设备上安装的 wxhook apk（com.butler.wxhook）**缺 `xposedmodule`/`xposedminversion`/
`xposeddescription` metadata**（只有 `assets/xposed_init` + `xposed_scope`），
而源码 `F:\ai-butler\mobile\wxhook` 的 AndroidManifest **已声明完整**。
判定：设备装的是**旧版构建**；即使 QQ 框架修复，wxhook 也需重装新版 apk 才能被
LSPosed 以标准方式识别加载（老式 xposed_init 在微信下已兼容加载成功，见 lspd 日志）。

## 六、状态

- [x] 根因定位（源码 + 真机双重验证）
- [ ] sepolicy.rule 修复（待用户同意）
- [ ] 真机验证 QQ 内 wxhook 加载
