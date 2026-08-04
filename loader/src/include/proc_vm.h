/**
 * proc_vm.h - 跨进程内存访问（无 ptrace 依赖）
 *
 * 使用 Linux process_vm_writev / process_vm_readv 系统调用
 * 直接读写目标进程的内存，完全绕过 ptrace。
 *
 * 参考 Zygisk-Next zygiskd 中的实现。
 *
 * 优势（与 ptrace 对比）：
 * - 不改变 TracerPid
 * - 不产生 ptrace 事件
 * - 对目标进程透明
 * - 无法通过传统反调试工具检测
 *
 * 前提条件：
 * - 需要 CAP_SYS_PTRACE 或 root 权限
 * - 目标进程的 /proc/pid/mem 可访问
 * - Linux 3.2+
 */

#ifndef PROC_VM_H
#define PROC_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>

/**
 * proc_vm_write — 直接写入目标进程内存
 *
 * @param pid          目标进程 PID
 * @param remote_addr  目标进程中的地址
 * @param local_buf    本地缓冲区（要写入的数据）
 * @param len          数据长度
 * @return             成功写入的字节数，失败返回 -1
 */
ssize_t proc_vm_write(pid_t pid, void *remote_addr,
                       const void *local_buf, size_t len);

/**
 * proc_vm_read — 直接读取目标进程内存
 *
 * @param pid          目标进程 PID
 * @param remote_addr  目标进程中的地址
 * @param local_buf    本地缓冲区（读取到此处）
 * @param len          要读取的长度
 * @return             成功读取的字节数，失败返回 -1
 */
ssize_t proc_vm_read(pid_t pid, const void *remote_addr,
                      void *local_buf, size_t len);

/**
 * proc_vm_write_chunked — 分块写入（大数据量）
 *
 * 当数据超过 proc_vm_writev 的单次限制时，自动分块写入。
 *
 * @param pid          目标进程 PID
 * @param remote_addr  目标地址
 * @param local_buf    本地数据
 * @param total_len    总长度
 * @return             成功返回 true，失败返回 false
 */
bool proc_vm_write_chunked(pid_t pid, void *remote_addr,
                            const void *local_buf, size_t total_len);

/**
 * proc_mem_open — 通过 /proc/pid/mem 进行后备访问
 *
 * 当 process_vm_writev 不可用时（旧内核），回退到 /proc/pid/mem。
 * 注意：这仍然不产生 ptrace 痕迹。
 *
 * @param pid   目标进程 PID
 * @param flags open 标志 (O_RDONLY, O_RDWR 等)
 * @return      文件描述符，失败返回 -1
 */
int proc_mem_open(pid_t pid, int flags);

/**
 * proc_mem_pwrite — 通过 /proc/pid/mem 写入
 *
 * @param fd      proc_mem_open 返回的文件描述符
 * @param buf     数据缓冲区
 * @param len     数据长度
 * @param offset  目标地址
 * @return        成功写入的字节数，失败返回 -1
 */
ssize_t proc_mem_pwrite(int fd, const void *buf, size_t len, off_t offset);

/**
 * proc_mem_pread — 通过 /proc/pid/mem 读取
 */
ssize_t proc_mem_pread(int fd, void *buf, size_t len, off_t offset);

/**
 * proc_vm_is_available — 检查 process_vm_writev 是否可用
 *
 * 通过尝试调用 syscall 来检测内核支持。
 *
 * @return 可用返回 true
 */
bool proc_vm_is_available(void);

#endif /* PROC_VM_H */
