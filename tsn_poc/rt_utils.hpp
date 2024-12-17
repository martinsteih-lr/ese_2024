#ifndef RT_UTILS_HPP
#define RT_UTILS_HPP

#include <malloc.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string_view>

//Only needed for PoC. This functionality is already provided within OSAL
namespace RT {
static void lock_memory() {
  if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
    perror("mlockall failed");
    exit(EXIT_FAILURE);
  }
  if (mallopt(M_TRIM_THRESHOLD, -1) == -1) {
    perror("mallopt failed");
    exit(EXIT_FAILURE);
  }
  if (mallopt(M_MMAP_MAX, 0) == -1) {
    perror("mallopt failed");
    exit(EXIT_FAILURE);
  }
}

static void prefault_stack() {
  struct rlimit limit;
  const int size = getrlimit(RLIMIT_STACK, &limit);
  unsigned char dummy[limit.rlim_cur];
  for (long i = 0; i < size; i += sysconf(_SC_PAGESIZE)) {
    dummy[i] = 1;
  }
}

static void prefault_heap() {
  constexpr size_t size(1024UL * 1024UL * 1024UL);
  std::vector<unsigned char> dummy(size);
  for (size_t i = 0; i < size;
       i += static_cast<uint64_t>(sysconf(_SC_PAGESIZE))) {
    dummy[i] = 1;
  }
}

static void set_realtime_priority(const int priority) {
  sched_param param;
  param.sched_priority = priority;
  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    perror("sched_setscheduler failed");
    exit(EXIT_FAILURE);
  }
}

static int set_cpuset(const std::string &cpusetName) {
  std::ofstream of{"/dev/cpuset/" + cpusetName + "/tasks", std::ios::app};
  of << getpid();
  return 0;
}
static void set_affinity(const int cpu) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  const pthread_t thread(pthread_self());
  if (pthread_setaffinity_np(thread, sizeof(mask), &mask) != 0) {
    exit(EXIT_FAILURE);
    // Handle error
  }
}
static void set_name(const std::string_view name) {
  const pthread_t thread(pthread_self());
  if (pthread_setname_np(thread, name.begin()) != 0) {
    exit(EXIT_FAILURE);
    // Handle error
  }
}
void setupRt(const int affinity, const std::string &cpusetName,
             const std::string_view threadName, const int priority) {
  RT::lock_memory();
  RT::prefault_stack();
  RT::prefault_heap();
  RT::set_realtime_priority(priority);
  RT::set_cpuset(cpusetName);
  RT::set_affinity(affinity);
  RT::set_name(threadName);
}
}  // namespace RT
#endif  // RT_UTILS_HPP