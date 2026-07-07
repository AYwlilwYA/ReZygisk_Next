/**
 * crt0.c - 最小化 C 运行时初始化
 *
 * 因为 payload 不链接 libc，所以需要自己的 _start 入口。
 * 此文件提供最小化的 CRT 初始化。
 *
 * 注意：此文件仅在"独立运行"模式下使用。
 * 当作为共享库被注入时，走的是 payload_entry 路径。
 */

#ifdef PAYLOAD_STANDALONE

/* 通过链接器脚本提供 */
extern int payload_entry(const char *path, unsigned int flags);

/* 全局偏移表指针 */
extern void *__global_pointer[];

__attribute__((force_align_arg_pointer))
void _start(void) {
    /* 独立模式下使用硬编码路径 */
    const char *default_path =
        "/data/adb/modules/rezygisk/bin/zygiskd64";

    payload_entry(default_path, 0);

    /* 如果 payload_entry 返回，调用 exit 系统调用 */
    __asm__ volatile(
        "mov x8, #93\n"  /* __NR_exit */
        "mov x0, #1\n"
        "svc #0\n"
    );
    __builtin_unreachable();
}

#endif /* PAYLOAD_STANDALONE */
