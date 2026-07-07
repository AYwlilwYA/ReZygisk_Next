#include <stdlib.h>
#include <string.h>

#include <sys/prctl.h>

#include "root_impl/common.h"
#include "companion.h"
#include "zygiskd.h"

#include "utils.h"
#include "zn_stealth.h"

int main(int argc, char *argv[]) {
  /* ReZygisk Next: 初始化隐藏性措施 */
  zn_stealth_init();

  /* 隐藏 cmdline: 用伪装进程名覆盖 argv[0] */
  if (argc > 0 && argv[0]) {
    char comm[16];
    if (prctl(PR_GET_NAME, comm, 0, 0, 0) == 0) {
      size_t len = strlen(comm);
      size_t orig_len = strlen(argv[0]);
      memset(argv[0], 0, orig_len);
      memcpy(argv[0], comm, len < orig_len ? len : orig_len);
    }
  }

  LOGI("Welcome to ReZygiskd%s", LP_SELECT("32", "64"));

  if (argc > 1) {
    if (strcmp(argv[1], "companion") == 0) {
      if (argc < 3) {
        LOGI("Usage: zygiskd companion <fd>");

        return 1;
      }

      int fd = atoi(argv[2]);
      companion_entry(fd);

      return 0;
    }

    else if (strcmp(argv[1], "version") == 0) {
      LOGI("ReZygisk Daemon %s", ZKSU_VERSION);

      return 0;
    }

    else if (strcmp(argv[1], "root") == 0) {
      root_impls_setup();

      struct root_impl impl;
      get_impl(&impl);

      char impl_name[LONGEST_ROOT_IMPL_NAME];
      stringify_root_impl_name(impl, impl_name);

      LOGI("Root implementation: %s", impl_name);

      return 0;
    }

    else {
      LOGI("Usage: zygiskd [companion|version|root]");

      return 0;
    }
  }

  if (switch_mount_namespace(1) == false) {
    LOGE("Failed to switch mount namespace");

    return 1;
  }
  root_impls_setup();
  zygiskd_start(argv);

  return 0;
}
