#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stddef.h>

// TSan-invisible barrier.
// Tests use it to establish necessary execution order in a way that does not
// interfere with tsan (does not establish synchronization between threads).
__typeof(pthread_barrier_wait) *barrier_wait;

void barrier_init(pthread_barrier_t *barrier, unsigned count) {
#if defined(__FreeBSD__)
  static const char libpthread_name[] = "libpthread.so";
#else
  static const char libpthread_name[] = "libpthread.so.0";
#endif

  if (barrier_wait == 0) {
    void *h = dlopen(libpthread_name, RTLD_LAZY);
    if (h == 0) {
      fprintf(stderr, "failed to dlopen %s, exiting\n", libpthread_name);
      exit(1);
    }
    barrier_wait = (__typeof(barrier_wait))dlsym(h, "pthread_barrier_wait");
    if (barrier_wait == 0) {
      fprintf(stderr, "failed to resolve pthread_barrier_wait, exiting\n");
      exit(1);
    }
  }
  pthread_barrier_init(barrier, 0, count);
}

// Default instance of the barrier, but a test can declare more manually.
pthread_barrier_t barrier;

void print_address(void *address) {
// On FreeBSD, the %p conversion specifier works as 0x%x and thus does not match
// to the format used in the diagnotic message.
#ifdef __x86_64__
  fprintf(stderr, "0x%012lx", (unsigned long) address);
#elif defined(__mips64)
  fprintf(stderr, "0x%010lx", (unsigned long) address);
#elif defined(__aarch64__)
  // AArch64 currently has 3 different VMA (39, 42, and 48 bits) and it requires
  // different pointer size to match the diagnostic message.
  const char *format = 0;
  unsigned long vma = (unsigned long)__builtin_frame_address(0);
  vma = 64 - __builtin_clzll(vma);
  if (vma == 39)
    format = "0x%010lx";
  else if (vma == 42)
    format = "0x%011lx";
  else {
    fprintf(stderr, "unsupported vma: %ul\n", vma);
    exit(1);
  }

  fprintf(stderr, format, (unsigned long) address);
#endif
}
