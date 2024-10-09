#include "argparse.h"
#include "utils.h"
#include <algorithm>
#include <bits/types/sigset_t.h>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <linux/prctl.h>
#include <ostream>
#include <string>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;
namespace chrono = std::chrono;

// globals
pid_t c_pid = 0;
int c_status;
std::time_t start_time;

// statistic
long max_rss = 0;
double cpu_time = 0;

void sig_block(int signo) {
  sigset_t ss;
  sigemptyset(&ss);
  sigaddset(&ss, signo);
  if (sigprocmask(SIG_BLOCK, &ss, NULL))
    perror("sigprocmask BLOCK failed");
}

// void sig_unblock(int signo) {
//   sigset_t ss;
//   sigemptyset(&ss);
//   sigaddset(&ss, signo);
//   if (sigprocmask(SIG_UNBLOCK, &ss, NULL))
//     perror("sigprocmask UNBLOCK failed");
// }

double getCpuTime(int pid) {
  std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
  if (stat) {
    std::string token;
    for (int i = 0; i < 13; i++) {
      stat >> token;
    }
    long ret = 0, tmp;
    for (int i = 0; i < 4; i++) {
      stat >> tmp;
      ret += tmp;
    }
    return (double)ret / sysconf(_SC_CLK_TCK);
  }
  return 0;
}

void killChildProcess(int sig) {
  sig_block(sig);
  kill(0, sig);
  while (true) {
    int ret = waitpid(c_pid, &c_status, WNOHANG);
    dbgs() << "waitpid: " << ret << "\n";
    if (ret == 0) {
      sleep(1);
      continue;
    }
    if (ret > 0) {
      if (WIFEXITED(c_status) | WIFSIGNALED(c_status) | WCOREDUMP(c_status)) {
        break;
      }
    }
    if (ret == -1) {
      perror("waitpid failed");
      exit(-1);
    }
  }
  // }
  dbgs() << "child process killed\n";
  return;
}

int _getMemoryUsage(int pid) {
  int total = 0, c_pid, mem, rss;
  std::error_code ec;
  for (const auto &entry :
       fs::directory_iterator("/proc/" + std::to_string(pid) + "/task", ec)) {
    std::ifstream children(entry.path() / "children");
    while (children >> c_pid) {
      total += _getMemoryUsage(c_pid);
    }
  }
  std::ifstream statm("/proc/" + std::to_string(pid) + "/statm");
  if (statm) {
    statm >> mem >> rss;
    total += rss;
  }
  return total;
}

long getMemoryUsage(int child_pid) {
  int ret = _getMemoryUsage(child_pid);
  return sysconf(_SC_PAGESIZE) * ret;
}

int report() {
  dbgs() << "print out info\n";
  // exited
  int ret_code;
  errs() << "child " << c_pid << " exit,";
  if (WIFSIGNALED(c_status)) {
    int signo = WTERMSIG(c_status);
    ret_code = 128 + signo;
    errs() << " term signal " << strsignal(signo) << "\n";
  } else if (WIFEXITED(c_status)) {
    ret_code = WEXITSTATUS(c_status);
    errs() << " exit code " << ret_code << "\n";
  }

  time_t wall_time = std::time(nullptr) - start_time;
  std::chrono::seconds elapsed_wall(wall_time);
  errs() << fmt::format(
      "Elapsed (wall clock) time (h:mm:ss or m:ss): {:02d}:{:02d}:{:02d}\n"
      "CPU time (seconds): {:.2f}\n"
      "Maximum resident set size (kbytes): {:d}\n",
      chrono::duration_cast<chrono::hours>(elapsed_wall).count(),
      chrono::duration_cast<chrono::minutes>(elapsed_wall).count() % 60,
      chrono::duration_cast<chrono::seconds>(elapsed_wall).count() % 60,
      cpu_time, max_rss >> 10);
  return ret_code;
}

void handler(int sig) {
  if (c_pid && (sig == SIGINT || sig == SIGTERM)) {
    killChildProcess(sig);
  }
  exit(report());
}

int main(int argc, char **argv) {
  signal(SIGINT, handler);
  signal(SIGTERM, handler);
  setsid();

  parse(argc, argv);

  start_time = std::time(nullptr);
  c_pid = fork();
  if (c_pid == -1) {
    perror("Fork failed");
    exit(-1);
  } else if (c_pid == 0) {
    // child, execv
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (execvp(cmd[0], cmd) == -1) {
      perror("execv failed");
      exit(-1);
    }
  } else {
    // parent, gather statistic and wait
    while (true) {
      int ret = waitpid(c_pid, &c_status, WNOHANG);
      if (ret == -1) {
        perror("waitpid failed");
        exit(-1);
      } else if (ret != 0) {
        // child status changed
        break;
      }
      // ret == 0, child still running
      cpu_time = std::max(cpu_time, getCpuTime(c_pid));
      long mem_in_b = getMemoryUsage(c_pid);
      float mem_in_g = (float)(mem_in_b >> 20) / (1 << 10);
      max_rss = std::max(max_rss, mem_in_b);
      if (mem_in_g > mem_limit) {
        errs() << fmt::format("Memory guard triggered, {:.2f}\n", mem_in_g);
        killChildProcess(SIGTERM);
        break;
      }
      usleep(1000000);
    }
    return report();
  }
}