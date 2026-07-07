#ifndef MONITOR_H
#define MONITOR_H

#include <stdbool.h>

void init_monitor(int argc, char **argv);

bool trace_zygote(int pid, bool tango_flag);

enum rezygiskd_command {
  START = 1,
  STOP = 2,
  EXIT = 3,

  /* sent from daemon */
  ZYGOTE64_INJECTED = 4,
  ZYGOTE32_INJECTED = 5,
  DAEMON64_SET_INFO = 6,
  DAEMON32_SET_INFO = 7,
  DAEMON64_SET_ERROR_INFO = 8,
  DAEMON32_SET_ERROR_INFO = 9
};

int send_control_command(enum rezygiskd_command cmd);

#endif /* MONITOR_H */