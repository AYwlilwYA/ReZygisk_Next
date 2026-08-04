/**
 * mremap_loader.h — 隐蔽模块加载器接口
 */

#ifndef MREMAP_LOADER_H
#define MREMAP_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * 隐蔽加载共享库到目标进程
 *
 * 使用 mremap + /proc/pid/mem 的组合方式，
 * 避免在目标进程的 /proc/pid/maps 中创建新的 VMA 条目。
 *
 * @param pid           目标进程 PID
 * @param lib_disk_path 磁盘上的 .so 文件路径
 * @param out_base      输出：远程基址
 * @param out_size      输出：库大小
 * @return              成功返回 true
 */
bool zn_mremap_load_library(pid_t pid, const char *lib_disk_path,
                              void **out_base, size_t *out_size);

/**
 * 清理已注入的代码
 *
 * 使用 MADV_DONTNEED/MADV_FREE 释放内存，
 * 比 munmap 更隐蔽。
 *
 * @param base 基地址
 * @param size 大小
 */
void zn_mremap_cleanup(void *base, size_t size);

#endif /* MREMAP_LOADER_H */
