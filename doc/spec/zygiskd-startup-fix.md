# Bug 记录：zygiskd 在 APatch/KSU 下无法启动

> 日期：2026-08-03
> 状态：已修复并真机验证
> 提交：5582beb
> 关联：`spec/app-view-hiding-survey.md`、`spec/module-memfd-fix.md`

---

## 一、为什么错（根因）

zygiskd 不是脚本启动的，而是 **monitor（zygisk-ptrace64）在捕获到 zygote exec 事件时懒启动**
（`loader/src/ptracer/monitor.c` 的 `ensure_daemon_created`，fork + execl）。

主力机 PGEM10（Android 16 / APatch）上 monitor 能起来但 zygiskd 完全不启动，
state.json 不生成。根因是**三处环境依赖**（与 socket 抽象化无关，git 对比确认新旧版本在这三处相同）：

1. **rezygisk.sh 无 zygote 重启**（`module/src/rezygisk.sh`）
   - `post-fs-data.sh`（Magisk 路径）有 `killall -USR2 zygote` 触发注入，但 `rezygisk.sh`（APatch/KSU 路径）**没有**。
   - APatch 的 post-fs-data.d 执行时 zygote 早已 exec 完 → monitor 再无 `PTRACE_EVENT_EXEC` → zygiskd 永远不被 spawn。
2. **apatch.c 的 PATH 依赖**（`zygiskd/src/root_impl/apatch.c`）
   - `apatch_get_existence` 要求 `getenv("PATH")` 含 `/data/adb/ap/bin`，否则判定 Inexistent。
   - spawn 后 zygiskd 的 PATH 不含该目录 → `get_impl` 返回 None → `zygiskd.c:275-288` 发错误并 **exit(EXIT_FAILURE)**，进程秒退。
   - 实际执行 apd 用的是绝对路径（`execv` 不查 PATH），PATH 检查是多余的脆弱 gate。
3. **monitor.c execl 相对路径**（`loader/src/ptracer/monitor.c`）
   - `execl("./bin/zygiskd64")` 相对路径依赖 CWD；`rezygisk.sh` 无 `cd "$MODDIR"`。
   - APatch 下 monitor 的 CWD 不是模块目录 → ENOENT → 一次失败后 `CHECK_DAEMON_EXIT` 置 `daemon_running=false`，之后彻底放弃。

**踩的坑**：初次误判为「socket 抽象化引入的问题」，派 agent 排查后确认 socket 抽象化的 spawn 分支
一字未改、失败路径被手动执行成功否定。真正根因是三处**环境依赖**——旧版（b305faa）能跑是因为当时
环境（CWD/PATH/zygote 时序）恰好正确，PGEM10 上新装时暴露。

## 二、修复

| 文件 | 改动 |
|------|------|
| `monitor.c` | spawn zygiskd/tracer 改用绝对路径 `/data/adb/modules/rezygisk/bin/...`，消除 CWD 依赖 |
| `rezygisk.sh` | 开头 `cd "$MODDIR"`；末尾 `sleep 2` + `killall -USR2 zygote64/zygote` 触发注入；`init_monitor` 残留哨兵改为删除而非跳过 |
| `apatch.c` | 删除 PATH gate，用绝对路径 `/data/adb/ap/bin/apd` 检测 APatch；加强 `-V` 输出解析 |

## 三、真机验证（PGEM10 / Android 16 / APatch）

- zygiskd64/zygiskd32 + LSPosed companion 全部启动，exe 指向 rezygisk/bin/
- state.json：monitor state=0、rezygiskd 64/32 state=1、zygote 64/32=1、memfd: ok
- 抽象 socket（@resmon_agent/@engine_state_ctl/@criticallog_evt）在位，`/proc/net/unix` 路径泄漏计数 0
- LSPosed 正常注入；zygote 重启修复被实际触发并生效（monitor 捕获 exec → handoff tracer → 注入成功）

## 四、遗留（非本次引入）

1. ~~**mount namespace EACCES**：zygiskd 持有的 mnt ns fd（`/proc/1872/fd/15`）app 进程打不开（SELinux 拒 13）。~~
   **已修复（2026-08-03，仅 sepolicy.rule）**：
   - 根因（真机 AVC 证据纠正原假设）：**不是 fork 后的 app 域**。scontext 实测为 `u:r:zygote:s0`（zygote 主线程 `comm="main"` 在 `rz_app_specialize_pre` 中调用 `update_mnt_ns`），tcontext 为 `u:r:magisk:s0`、tclass=lnk_file、perm=read。原因是被打开的 `/proc/<zygiskd_pid>/fd/N` 符号链接 inode 被标记为**持有该 fd 的 zygiskd 进程域**（`magisk`，zygiskd 伪装 netd/logd/installd 均跑在此域），而 `sepolicy.rule` 已有 `su`/`ksu` 类型的 `lnk_file read`，唯独漏了 `magisk` 类型。
   - 修复：`module/src/sepolicy.rule` 新增 `allow zygote magisk lnk_file read`。
   - 验证（PGEM10 / Android16 / APatch）：`magiskpolicy --print-rules` 确认规则已加载；启动 denylist 应用（com.zhenxi.hunter、io.github.vvb2060.mahoshojo）后 dmesg 全启动 **0 条** zygote/magisk/nsfs AVC 拒绝（修复前每 ~10s 一条），应用各自进入独立 clean mount namespace、mountinfo 无模块挂载泄漏，state.json/LSPosed 正常。
2. **wx_syshook arm64 注入失败**：单模块问题，其目标通常是 32 位微信。
