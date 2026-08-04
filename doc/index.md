# 文档索引

> 所有开发文档统一存放于本仓库 `/doc` 下。
> 索引建立于 2026-08-03，之后新增文档需在此登记。

## 规格 / 梳理 (doc/spec)

| 文档 | 说明 | 日期 | 状态 |
|------|------|------|------|
| [应用视角隐藏全面梳理](spec/app-view-hiding-survey.md) | app 进程自身视角的隐藏面盘点：注入链时间线、现有机制覆盖度、12 类检测面对照、Top 5 暴露点、加固方向；第 8 节记录 memfd 修复后剩余检测点与 socket 修复/MAP_SHARED 回退 | 2026-08-03 | 梳理+修复追踪 |
| [模块库 VMA path 暴露修复](spec/module-memfd-fix.md) | Bug 记录：模块 .so 以真实 /data/adb/modules 路径残留 app maps（unmount 管不到 VMA path）；修复为 memfd 伪装加载（jit-cache）+ 回退分支 + 降级标记 | 2026-08-03 | 已修复已验证 |
| [zygiskd 启动修复](spec/zygiskd-startup-fix.md) | Bug 记录：zygiskd 在 APatch/KSU 下不启动（monitor 收不到 exec 事件 / PATH 依赖 / CWD 相对路径三处环境问题）；修复 + 真机验证 | 2026-08-03 | 已修复已验证 |
| [zygiskd mnt ns fd EACCES 修复](spec/zygiskd-mnt-ns-eacces-fix.md) | Bug 记录：zygisk-core 在 app 进程 open /proc/&lt;zygiskd&gt;/fd/N 被 SELinux EACCES（magisk 域缺 dir search + file read），导致 denylist 分支（QQ）LSPosed 框架不初始化；修复方案 + 真机验证 | 2026-08-04 | 待修复 |

## 进行中 / 待办

- [ ] 实施 `spec/module-memfd-fix.md`：memfd 伪装加载（agent 进行中）
- [ ] 真机验证：app maps 显示 `/memfd:jit-cache (deleted)`、模块功能正常
- [ ] 依据 `spec/app-view-hiding-survey.md` 第 6 节评估后续加固（P1/P2）
