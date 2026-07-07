# ReZygisk Next Payload Makefile
# 构建最小化的纯 syscall 引导代码

ROOT_DIR := ..
include ../common.mk

BUILD_DIR ?= $(ROOT_DIR)/build

OBJ_ROOT = $(BUILD_DIR)/obj/$(BUILD_TYPE)/payload
OBJ_DIR = $(OBJ_ROOT)/$(ARCH)

# Payload 源文件
PAYLOAD_SRCS = src/payload_entry.c
PAYLOAD_OBJS = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(PAYLOAD_SRCS))

# 输出
LIBPAYLOAD = $(OBJ_DIR)/libpayload.so
LIBPAYLOAD_STRIPPED = $(OBJ_DIR)/stripped/libpayload.so

# Payload 特殊编译标志
# - 不链接 libc (使用 -nostdlib)
# - 不链接 libgcc (使用 -nostartfiles)
# - 隐藏所有符号 (-fvisibility=hidden)
PAYLOAD_CFLAGS = -std=c11 -D_GNU_SOURCE \
	-fno-stack-protector -fomit-frame-pointer \
	-fvisibility=hidden -ffunction-sections -fdata-sections \
	-Os -DPAYLOAD_SHARED \
	-Iinclude -I$(SYSROOT)/usr/include

PAYLOAD_LDFLAGS = -nostdlib -nostartfiles \
	-Wl,--gc-sections -Wl,--build-id=sha1 \
	-Wl,--no-undefined -Wl,--fatal-warnings \
	-Wl,-soname,libpayload.so \
	-Wl,--exclude-libs,ALL -Wl,--strip-all

.PHONY: all arch clean

all:
	@for arch in $(ARCHS); do \
		$(MAKE) BUILD_TYPE=$(BUILD_TYPE) BUILD_DIR=$(BUILD_DIR) \
			ARCH=$$arch arch; \
	done

arch: $(LIBPAYLOAD_STRIPPED)

clean:
	rm -rf $(BUILD_DIR)/obj/$(BUILD_TYPE)/payload

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC_ARCH) $(PAYLOAD_CFLAGS) -fPIC -c $< -o $@

$(LIBPAYLOAD): $(PAYLOAD_OBJS)
	@mkdir -p $(dir $@)
	$(CC_ARCH) -fPIC -shared $(PAYLOAD_LDFLAGS) $^ -o $@

$(LIBPAYLOAD_STRIPPED): $(LIBPAYLOAD)
	@mkdir -p $(dir $@)
	cp $< $@
	$(STRIP) --strip-all $@
