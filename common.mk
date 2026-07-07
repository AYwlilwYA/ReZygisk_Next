ROOT_DIR ?= .
BUILD_TYPE ?= debug
API_LEVEL ?= 25
ARCHS ?= arm64-v8a armeabi-v7a x86 x86_64
ARCH ?= arm64-v8a

VER_NAME ?= v2.0.0
VER_CODE ?= $(shell echo $$(( $$(git -C "$(ROOT_DIR)" rev-list HEAD --count 2>/dev/null || echo 1) + 99 )))
COMMIT_HASH ?= $(shell git -C "$(ROOT_DIR)" rev-parse --verify --short HEAD 2>/dev/null || echo unknown)

MIN_APATCH_VERSION ?= 10655
MIN_KSU_VERSION ?= 10940
MIN_KSUD_VERSION ?= 11425
MIN_MAGISK_VERSION ?= 26402

MODULE_ID ?= rezygisk
MODULE_NAME ?= ReZygisk Next

# Windows NDK 路径配置
NDK_ROOT ?= D:/Android/AndroidNDK/Android-NDK-r27c
ANDROID_HOME ?= D:/Android/android-sdk
NDK_PATH ?= $(NDK_ROOT)
# 自动检测主机平台，选择对应 NDK 预构建工具链
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Linux)
    HOST_PLATFORM := linux-x86_64
else ifeq ($(UNAME_S),Darwin)
    HOST_PLATFORM := darwin-x86_64
else
    HOST_PLATFORM := windows-x86_64
endif
TOOLCHAIN = $(NDK_PATH)/toolchains/llvm/prebuilt/$(HOST_PLATFORM)
SYSROOT = $(TOOLCHAIN)/sysroot

ifeq ($(TERMUX_VERSION),)
	CC = $(TOOLCHAIN)/bin/clang
	AR = $(TOOLCHAIN)/bin/llvm-ar
	STRIP = $(TOOLCHAIN)/bin/llvm-strip
else
	CC = clang
	AR = llvm-ar
	STRIP = llvm-strip
endif

BUILD_DIR ?= $(ROOT_DIR)/build

TARGET_arm64-v8a = aarch64-linux-android$(API_LEVEL)
TARGET_armeabi-v7a = armv7a-linux-androideabi$(API_LEVEL)
TARGET_x86 = i686-linux-android$(API_LEVEL)
TARGET_x86_64 = x86_64-linux-android$(API_LEVEL)

CC_ARCH = $(CC) --target=$(TARGET_$(ARCH)) --sysroot=$(SYSROOT)

NDK_CFLAGS = -DANDROID -fdata-sections -ffunction-sections -funwind-tables \
	-fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 \
	-Wformat -Werror=format-security

# ======================================================
# ReZygisk Next 混淆配置
# 启用方式: make OBFUSCATE=1 BUILD_TYPE=release
# 需要预先准备好 OLLVM/Hikari 工具链
# ======================================================
ifeq ($(OBFUSCATE),1)
    # 控制流平坦化 (Control Flow Flattening)
    OBF_CFLAGS += -mllvm -fla
    # 字符串加密 (String Encryption)
    OBF_CFLAGS += -mllvm -sobf
    # 指令替换 (Instruction Substitution)
    OBF_CFLAGS += -mllvm -sub
    # 基本块分割 (Basic Block Splitting)
    OBF_CFLAGS += -mllvm -split
    # 虚假控制流 (Bogus Control Flow)
    OBF_CFLAGS += -mllvm -bcf
    # 函数包装 (Function Wrapper)
    OBF_CFLAGS += -mllvm -fwra
else
    OBF_CFLAGS =
endif
