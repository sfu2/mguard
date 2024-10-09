#include "argparse.h"
#include "utils.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>

char **cmd = {};
int mem_limit = 32;

const char *HELP_MSG =
    "Usage: mguard [-m <mem limit in GB, default: 32>] <command>\n";

void parse(int argc, char **argv) {
  if (argc < 2) {
    std::cout << HELP_MSG;
    exit(0);
  }
  int i = 1;
  // deal with options
  for (; i < argc; ++i) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      std::cout << HELP_MSG;
      exit(0);
    } else if (strcmp(argv[i], "-m") == 0) {
      if (i + 1 < argc) {
        errno = 0;
        char *end;
        mem_limit = strtol(argv[++i], &end, 10);
        if (!errno && argv[i] == end) {
          errno = EINVAL;
        }
      } else {
        errno = EINVAL;
      }
      if (errno) {
        errs() << "Invalid memory limit\n";
        exit(EXIT_FAILURE);
      }
    } else {
      break;
    }
  }
  // deal with command
  int cmd_len = argc - i;
  if (cmd_len <= 0) {
    errs() << "No command specified\n";
    exit(EXIT_FAILURE);
  }
  // for (; i < argc; ++i) {
  //   outs() << argv[i] << "\n";
  // }
  cmd = new char *[cmd_len + 1];
  memcpy(cmd, argv + i, cmd_len * sizeof(char *));
  cmd[cmd_len] = nullptr;
  errs() << "Executing: ";
  for (i = 0; i < cmd_len; ++i) {
    errs() << cmd[i] << " ";
  }
  errs() << "\n";
}