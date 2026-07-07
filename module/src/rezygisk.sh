#!/system/bin/sh

# INFO: This script is executed by APatch/KernelSU post-fs-data.d mechanism.
#       It restores module.prop and starts the ReZygisk Next monitor.

MODDIR=/data/adb/modules/rezygisk
TMP_PATH=/data/adb/rezygisk

# 恢复干净的 module.prop
cp "$MODDIR/module.prop.bak" "$MODDIR/module.prop"

# 避免重复启动：如果已运行则跳过
if [ -f "$TMP_PATH/init_monitor" ]; then
  exit 0
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

exit 0
