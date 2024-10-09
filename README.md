# MGuard

Simple command line utility to enforce limit on RSS, with no need of kernel support and privileged permission.

Why not `ulimit`? `ulimit -m` stopped working since kernel 2.4.30.

Why not cgroup? cgroup is great, but 1. it requires kernel support and 2. only cgroup v1 supports rootless.

## Build

```bash
mkdir build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target mguard -- -j
```

## Usage

```bash
# usage
$ mguard 
Usage: mguard [-m <mem limit in GB, default: 32>] <command>
# kill if oom
$ mguard -m 10 tail /dev/zero 
Executing: tail /dev/zero 
Memory guard triggered, 11.25
child 53353 exit, term signal Terminated
Elapsed (wall clock) time (h:mm:ss or m:ss): 00:00:06
CPU time (seconds): 4.99
Maximum resident set size (kbytes): 11802492
# print output as expected
$ mguard -m 10 ls
Executing: ls 
argparse.cpp  argparse.h CMakeLists.txt  main.cpp  README.md utils.cpp  utils.h
child 53724 exit, exit code 0
Elapsed (wall clock) time (h:mm:ss or m:ss): 00:00:01
CPU time (seconds): 0.00
Maximum resident set size (kbytes): 192
# make it quiet
$ mguard -m 10 ls 2>/dev/null
argparse.cpp  argparse.h CMakeLists.txt  main.cpp  README.md utils.cpp  utils.h
```