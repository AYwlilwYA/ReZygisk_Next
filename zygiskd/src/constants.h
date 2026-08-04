#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdbool.h>
#include <stdint.h>

#define PROCESS_NAME_MAX_LEN 256 + 1

#define ZYGOTE_INJECTED LP_SELECT(5, 4)
#define DAEMON_SET_INFO LP_SELECT(7, 6)
#define DAEMON_SET_ERROR_INFO LP_SELECT(9, 8)

/* ====================================================
 * 抽象 socket 名（伪装，app 视角 /proc/net/unix 只见 @name）
 * 必须与 loader/src/include/zn_strings.h 中
 * ZN_SOCKET_DAEMON_32 / ZN_SOCKET_DAEMON_64 / ZN_SOCKET_MONITOR
 * 的值保持一致。
 * 命名原则：短、像系统/vendor 服务抽象名，不含 rezygisk/zygisk/cp。
 * ==================================================== */
#define ZN_DAEMON_SOCKET_32 "engine_state_ctl"   /* 原 cp32.sock */
#define ZN_DAEMON_SOCKET_64 "resmon_agent"       /* 原 cp64.sock */
#define ZN_MONITOR_SOCKET   "criticallog_evt"    /* 原 init_monitor */

enum DaemonSocketAction {
  ZygoteInjected         = 0,
  GetProcessFlags        = 1,
  GetInfo                = 2,
  ReadModules            = 3,
  RequestCompanionSocket = 4,
  GetModuleDir           = 5,
  ZygoteRestart          = 6,
  UpdateMountNamespace   = 7,
  RemoveModule           = 8
};

enum ProcessFlags: uint32_t {
  PROCESS_GRANTED_ROOT = (1u << 0),
  PROCESS_ON_DENYLIST = (1u << 1),
  PROCESS_IS_MANAGER = (1u << 27),
  PROCESS_ROOT_IS_APATCH = (1u << 28),
  PROCESS_ROOT_IS_KSU = (1u << 29),
  PROCESS_ROOT_IS_MAGISK = (1u << 30),
  PROCESS_IS_FIRST_STARTED = (1u << 31)
};

enum RootImplState {
  Supported,
  TooOld,
  Inexistent,
  Abnormal
};

enum MountNamespaceState {
  Clean,
  Mounted
};

#endif /* CONSTANTS_H */
