/* One mode 6 decoder build per variant header, with phase timers through VCN_LC_TICK.
   cc -O3 -DPFX=name -DHDR='"name.h"' -c tu.c */
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#if defined(__APPLE__)
static inline uint64_t tu_now(void) { return clock_gettime_nsec_np(CLOCK_UPTIME_RAW); }
#else
static inline uint64_t tu_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
#endif
#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)
uint64_t CAT(PFX, _tt)[16];
int CAT(PFX, _prof);
static uint64_t tu_t0;
#define VCN_LC_TICK(k)                                    \
  do {                                                    \
    if (CAT(PFX, _prof)) {                                \
      const uint64_t t_ = tu_now();                       \
      if ((k) >= 0) CAT(PFX, _tt)[(k) & 15] += t_ - tu_t0; \
      tu_t0 = t_;                                         \
    }                                                     \
  } while (0)
#include HDR
int CAT(PFX, _enc)(const uint8_t *s, uint8_t *d, size_t cap, size_t *n) { return (int)vcn_encode_lossless_chunk(s, d, cap, n); }
int CAT(PFX, _dec)(const uint8_t *e, size_t n, uint8_t *d) { return (int)vcn_decode(e, n, d, VCN_VOXELS); }
const char *CAT(PFX, _kern)(void) { return vcn_kernels(); }
size_t CAT(PFX, _bound)(void) { return VCN_ENCODE_BOUND; }
