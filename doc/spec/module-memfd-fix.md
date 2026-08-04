# Bug 记录：模块库 VMA path 暴露 → memfd 伪装加载修复

> 日期：2026-08-03
> 状态：实施中
> 关联：`spec/app-view-hiding-survey.md` Top 1 暴露点

---

## 一、为什么错（根因）

模块 .so 由自定义 linker csoloader 以**文件映射**加载进 zygote，fork 到每个 app：

```
linker.c:691  int fd = open("/data/adb/modules/<mod>/zygisk/<abi>.so", O_RDONLY|O_CLOEXEC)
linker.c:648  mmap(..., MAP_FIXED|MAP_PRIVATE, fd, ...)   ← file-backed VMA
linker.c:789  close(fd)                                    ← 只减 fd 引用
```

- `mmap` 成功时内核 `vma->vm_file = get_file(file)`，VMA **永久持有一份文件引用**，close 后
  `struct file` / inode / dentry / vfsmount 全部存活。
- `/proc/self/maps` 的路径由 `seq_file_path()` 用 **open 时捕获的 `f_path`** 解析，
  **与读取者进程的 mount namespace 无关**。
- 因此 denylist unmount（`hook.c:191-225` + `zygiskd/src/utils.c:745-874`）只能隐藏
  `/data/adb` 的**文件系统层可见性**，**对 maps 里的 VMA path 完全无效**。

**结论**：普通 app 和 denylist app 的 `/proc/self/maps` 都会出现完整的
`/data/adb/modules/<mod>/zygisk/<abi>.so`，一抓一个准。

## 二、怎么错的（修复前状态）

- 模块 .so 永久残留（`csoloader_abandon` 不 unmap，见 `csoloader.c:95` / `linker.c:586`）。
- maps 显示真实路径字符串 → 最强检测特征。
- dev/inode 与磁盘文件一致 → 检测方可通过 inode 交叉关联磁盘。

## 三、踩的坑 / 认知盲点

1. **误以为 denylist unmount 能覆盖一切 app 可见性**。事实：unmount 只影响文件系统层，
   VMA path 是 open 时捕获的，与 namespace 无关。这是本次梳理的核心教训。
2. **以为"藏自身"（libzygisk.so 自卸）就够了**，忽略了模块 .so 才是真正永久残留的大头。
3. 原版 ReZygisk 没有处理模块库路径（原版模块也是 file-backed 真实路径）——这是我们
   超越原版的点。

## 四、修复方案：memfd 伪装加载

与生态领导者 Zygisk Next 一致（其 daemon 把模块读入 memfd，名 "jit-cache"，app maps 显示
`/memfd:jit-cache (deleted)`，与 ART JIT cache memfd 混淆）。

**改动范围**：仅 `loader/src/external/csoloader/src/linker.c` 单点。

| 步骤 | 位置 |
|------|------|
| open 成功后 fstat 拿 size | `linker.c:691` 后 |
| `memfd_create("jit-cache", MFD_CLOEXEC)` + ftruncate | 同上 |
| 原 fd 内容逐字节拷入 memfd（pread 循环） | 同上 |
| 该 .so 后续所有读取（header/phdrs/段 mmap/重定位）改走 memfd fd | `linker.c:648` 及加载流程 |
| 加载完 close 原 fd；错误路径同时 close 两 fd | `linker.c:789` 及各错误分支 |
| 可选 F_ADD_SEALS | — |

协议/daemon/zygiskd 零改动。主模块和依赖库都走 `linker_load_library_manually`
（`linker.c:2129`），单点覆盖全部。

## 五、对模块语义的影响（已评估）

| 模块行为 | 影响 |
|---------|------|
| `get_module_dir`（资源访问，daemon 回 dirfd） | 无（与 .so 是否 memfd 无关） |
| `connect_companion` | 无 |
| PLT hook 按 dev/inode 匹配**其他库** | 无 |
| 按 dev/inode 匹配**自己** | inode 变为 memfd shmem，极小影响 |
| 模块自己扫 maps 找自身路径反推模块目录 | **受损**（应改用 get_module_dir，非标准做法） |
| `dl_iterate_phdr` / `dladdr` | 不变（本就未注册进系统 solist） |

**附带好处**：模块映射 dev/inode 与磁盘脱钩，检测方"maps inode ↔ 磁盘文件"交叉关联失效。

## 六、真机验证项

- [x] memfd 上 PROT_EXEC 的 mmap 是否过 SELinux（zygote 域验证通过：`r-xp ... /memfd:jit-cache (deleted)` 正常映射；untrusted_app 域未单独测，Zygisk Next 生产在用）
- [x] app 的 `/proc/self/maps` 显示 `/memfd:jit-cache (deleted)`，不再出现 `/data/adb/modules`
  - 证据（PDEM30 / Android 15 / APatch，2026-08-03 debug 包）：
    - zygote64 PID 1109：`0x7ec2888000 r-xp /memfd:jit-cache (deleted)`（64 位模块库）
    - zygote32 PID 1112：`e6a38000 r-xp /memfd:jit-cache (deleted)`（32 位模块库）
    - system_server PID 1700：`jit-cache` 计数 6，**`/data/adb/modules` 计数 0**，`libzygisk` 计数 0（自卸成功）
- [x] 模块功能正常（LSPosed 通过 memfd 加载成功，`state.json` rezygiskd 64/32 state=1，modules=["zygisk_lsposed"]）
- [x] 4 架构（arm64/arm/x86/x86_64）编译通过

## 六之二、真机验证记录（2026-08-03）

- 设备：PDEM30 / Android 15 (SDK 35) / APatch，su context `u:r:magisk:s0`（APatch su）
- 安装：`apd module install <debug zip>`（走 modules_update，重启 merge 生效）
- 状态：monitor state=0，zygiskd 64/32 state=1，zygote 64/32 注入成功，LSPosed 加载正常

## 七、后续增强（可选）

- memfd 方案稳定后评估「匿名加载」（完全去 memfd 名与 (deleted) 标记，隐蔽性最强）。
- 处理 survey Top 2：libzygisk.so 在 fork→ART 卸载窗口期的暴露。
