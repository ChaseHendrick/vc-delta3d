// Mode 6 decode speed by variant, against vc-delta3d LPC, in one process on one thread.
//   main DIR [reps]   (DIR holds 128^3 uint8 *.raw chunks)
// Every variant must encode the same bytes as the first and decode every chunk exactly.
#include <vc_delta3d/codec.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define DECL(P)                                                                 \
  extern "C" {                                                                  \
  extern uint64_t P##_tt[16];                                                   \
  extern int P##_prof;                                                          \
  int P##_enc(const uint8_t*, uint8_t*, size_t, size_t*);                       \
  int P##_dec(const uint8_t*, size_t, uint8_t*);                                \
  const char* P##_kern(void);                                                   \
  size_t P##_bound(void);                                                       \
  }
#include "variants.inc"
#undef V
struct Var {
  const char* name;
  uint64_t* tt;
  int* prof;
  int (*enc)(const uint8_t*, uint8_t*, size_t, size_t*);
  int (*dec)(const uint8_t*, size_t, uint8_t*);
  const char* (*kern)(void);
  size_t (*bound)(void);
};
#define V(P) DECL(P)
VARIANTS
#undef V
#define V(P) {#P, P##_tt, &P##_prof, P##_enc, P##_dec, P##_kern, P##_bound},
static Var vars[] = {VARIANTS};
#undef V
static const int NVAR = sizeof vars / sizeof vars[0];

static double now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static uint64_t fnv(const uint8_t* p, size_t n, uint64_t h = 1469598103934665603ull) {
  for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  if (argc < 2) return std::fprintf(stderr, "usage: main DIR [reps]\n"), 1;
  const int reps = argc > 2 ? std::atoi(argv[2]) : 7;
  const size_t NV = 128 * 128 * 128;
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(argv[1]))
    if (e.path().extension() == ".raw") files.push_back(e.path());
  std::sort(files.begin(), files.end());
  std::vector<std::vector<uint8_t>> src;
  for (const auto& f : files) {
    std::vector<uint8_t> b(NV);
    std::ifstream in(f, std::ios::binary);
    in.read(reinterpret_cast<char*>(b.data()), NV);
    if (in.gcount() == (std::streamsize)NV) src.push_back(std::move(b));
  }
  const size_t nc = src.size();
  if (!nc) return std::fprintf(stderr, "no chunks\n"), 1;
  std::printf("%zu chunks; kernels:", nc);
  for (int v = 0; v < NVAR; v++) std::printf(" %s=%s", vars[v].name, vars[v].kern());
  std::printf("\n");

  // streams from the first variant; every other variant must write the same bytes
  std::vector<std::vector<uint8_t>> enc(nc);
  size_t raw = nc * NV, m6 = 0, lpcb = 0;
  uint64_t dig = 1469598103934665603ull;
  for (size_t i = 0; i < nc; i++) {
    std::vector<uint8_t> e(vars[0].bound());
    size_t n = 0;
    if (vars[0].enc(src[i].data(), e.data(), e.size(), &n)) return std::fprintf(stderr, "encode failed\n"), 1;
    e.resize(n), m6 += n, dig = fnv(e.data(), n, dig);
    enc[i] = std::move(e);
  }
  int bad = 0;
  for (int v = 1; v < NVAR; v++)
    for (size_t i = 0; i < nc && i < 8; i++) {
      std::vector<uint8_t> e(vars[v].bound());
      size_t n = 0;
      if (vars[v].enc(src[i].data(), e.data(), e.size(), &n) || n != enc[i].size() || std::memcmp(e.data(), enc[i].data(), n))
        bad++, std::printf("ENCODE MISMATCH variant %s chunk %zu\n", vars[v].name, i);
    }
  std::vector<std::vector<std::byte>> lenc(nc);
  for (size_t i = 0; i < nc; i++) {
    lenc[i] = vc_delta3d::compress({reinterpret_cast<const std::byte*>(src[i].data()), NV}, {128, 128, 128}, 1, 1,
                                   vc_delta3d::Predictor::Lpc);
    lpcb += lenc[i].size();
  }
  std::vector<uint8_t> out(NV);
  for (int v = 0; v < NVAR; v++)
    for (size_t i = 0; i < nc; i++) {
      std::memset(out.data(), 0x5A, NV);
      if (vars[v].dec(enc[i].data(), enc[i].size(), out.data()) || std::memcmp(out.data(), src[i].data(), NV))
        bad++, std::printf("DECODE MISMATCH variant %s chunk %zu\n", vars[v].name, i);
    }
  std::printf("ratio: mode 6 %.4f, LPC %.4f; mode 6 stream digest %016llx; bad %d\n", (double)raw / m6, (double)raw / lpcb,
              (unsigned long long)dig, bad);

  // whole decode, best of reps, variants interleaved so they share the machine's state
  std::vector<double> best(NVAR + 1, 1e30);
  std::vector<std::vector<double>> all(NVAR + 1);
  for (int r = 0; r < reps; r++) {
    for (int v = 0; v <= NVAR; v++) {
      const double t0 = now();
      if (v < NVAR)
        for (size_t i = 0; i < nc; i++) vars[v].dec(enc[i].data(), enc[i].size(), out.data());
      else
        for (size_t i = 0; i < nc; i++)
          vc_delta3d::decompressInto({lenc[i].data(), lenc[i].size()}, {reinterpret_cast<std::byte*>(out.data()), NV});
      const double dt = now() - t0;
      best[v] = std::min(best[v], dt), all[v].push_back(dt);
    }
  }
  const double mb = raw / 1e6;
  for (int v = 0; v <= NVAR; v++) {
    auto s = all[v];
    std::sort(s.begin(), s.end());
    const double med = s[s.size() / 2];
    std::printf("%-10s dec %7.1f MB/s median [best %7.1f]  x%.3f of LPC (median)\n", v < NVAR ? vars[v].name : "lpc",
                mb / med, mb / best[v], (mb / med) / (mb / [&] { auto t = all[NVAR]; std::sort(t.begin(), t.end()); return t[t.size() / 2]; }()));
  }
  // phases, ns per voxel, best of 3
  static const char* ph[5] = {"rowsetup", "syms", "transp", "rec", "writeback"};
  for (int v = 0; v < NVAR; v++) {
    double bp[5] = {1e30, 1e30, 1e30, 1e30, 1e30};
    *vars[v].prof = 1;
    for (int r = 0; r < 3; r++) {
      std::memset(vars[v].tt, 0, 16 * sizeof(uint64_t));
      for (size_t i = 0; i < nc; i++) vars[v].dec(enc[i].data(), enc[i].size(), out.data());
      for (int k = 0; k < 5; k++) bp[k] = std::min(bp[k], (double)vars[v].tt[k] / raw);
    }
    *vars[v].prof = 0;
    std::printf("%-10s ns/voxel:", vars[v].name);
    double sum = 0;
    for (int k = 0; k < 5; k++) std::printf(" %s %.3f", ph[k], bp[k]), sum += bp[k];
    std::printf("  sum %.3f\n", sum);
  }
  return bad ? 2 : 0;
}
