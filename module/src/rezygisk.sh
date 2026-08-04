#!/system/bin/sh

# INFO: This script is executed by APatch/KernelSU post-fs-data.d mechanism.
#       It restores module.prop and starts the ReZygisk Next monitor.

MODDIR=/data/adb/modules/rezygisk
TMP_PATH=/data/adb/rezygisk

# INFO: 确保 CWD 为模块目录。monitor 懒启动 zygiskd/tracer 依赖相对路径
#       ./bin/zygiskd64 等，若脚本由 post-fs-data.d 从 / 启动会 exec 失败。
cd "$MODDIR"

# 恢复干净的 module.prop
cp "$MODDIR/module.prop.bak" "$MODDIR/module.prop"

# INFO: 旧版残留哨兵：新实现改用抽象 socket（@criticallog_evt），不再创建
#       init_monitor 文件。若旧设备残留该文件，删除后继续，避免永远跳过
#       monitor 启动。去重由 monitor 自身处理（重复 bind 抽象 socket 失败、
#       且 claim_init_tracer 检测到已有 monitor 后自动退出）。
if [ -f "$TMP_PATH/init_monitor" ]; then
  rm -f "$TMP_PATH/init_monitor"
fi

# 环境准备
rm -rf "$TMP_PATH"
mkdir -p "$TMP_PATH"
chmod 555 "$TMP_PATH"
chcon u:object_r:system_file:s0 "$TMP_PATH" 2>/dev/null || true

# 检测架构并启动 monitor
CPU_ABIS=$(getprop ro.product.cpu.abilist)
if echo "$CPU_ABIS" | grep -qE "arm64-v8a|x86_64"; then
  "$MODDIR/bin/zygisk-ptrace64" monitor &
else
  "$MODDIR/bin/zygisk-ptrace32" monitor &
fi

# INFO: 对齐 post-fs-data.sh：等待 monitor 就绪后重启 zygote，触发 exec 事件，
#       让 monitor 捕获并懒启动 zygiskd。否则 APatch/KSU 路径下 zygote 早已
#       exec 完毕，monitor 再无 exec 事件，zygiskd 永远不会被 spawn。
sleep 2
if echo "$CPU_ABIS" | grep -qE "arm64-v8a|x86_64"; then
  killall -USR2 zygote64 2>/dev/null || true
fi
killall -USR2 zygote 2>/dev/null || true

exit 0
