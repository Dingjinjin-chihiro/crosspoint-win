#include "sim_trace.h"

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
FILE* g_file = nullptr;
uint64_t g_count = 0;
uint64_t g_max = 0;
bool g_busy = false;
constexpr uint64_t kFlushEvery = 65536;

void record(char kind, void* this_fn) {
  // CRT + SDL only (both are external DLLs, never instrumented), and the
  // caller marks this file no-instrument, so this cannot recurse into itself.
  const uint64_t addr = reinterpret_cast<uint64_t>(this_fn);
  std::fprintf(g_file, "%c %u %llx\n", kind, static_cast<unsigned>(SDL_GetTicks()),
               static_cast<unsigned long long>(addr));
  if ((++g_count & (kFlushEvery - 1)) == 0) std::fflush(g_file);
}
}  // namespace

// A fixed symbol the analyzer uses to undo ASLR: its runtime address (written
// into the trace header) minus its file address (from nm) is the load delta.
void sim_trace_anchor(void) {}

extern "C" __attribute__((no_instrument_function)) void __cyg_profile_func_enter(void* this_fn, void*) {
  if (!g_file || g_busy) return;
  if (g_max && g_count >= g_max) return;
  g_busy = true;
  record('E', this_fn);
  g_busy = false;
}

extern "C" __attribute__((no_instrument_function)) void __cyg_profile_func_exit(void* this_fn, void*) {
  if (!g_file || g_busy) return;
  if (g_max && g_count >= g_max) return;
  g_busy = true;
  record('X', this_fn);
  g_busy = false;
}

void sim_trace_init(void) {
  const char* enabled = std::getenv("CROSSPOINT_TRACE");
  if (!enabled) return;
  const char* maxEnv = std::getenv("CROSSPOINT_TRACE_MAX");
  g_max = maxEnv ? std::strtoull(maxEnv, nullptr, 10) : 0;
  const char* pathEnv = std::getenv("CROSSPOINT_TRACE_FILE");
  const char* path = pathEnv ? pathEnv : "trace.log";
  g_file = std::fopen(path, "wb");
  if (!g_file) {
    std::printf("[TRACE] could not open %s\n", path);
    return;
  }
  std::setvbuf(g_file, nullptr, _IOFBF, 1 << 20);
  std::fprintf(g_file, "# anchor 0x%llx\n",
               static_cast<unsigned long long>(reinterpret_cast<uint64_t>(&sim_trace_anchor)));
  std::printf("[TRACE] tracing to %s (max %llu records)\n", path, static_cast<unsigned long long>(g_max));
  std::fflush(stdout);
}

void sim_trace_flush(void) {
  if (!g_file) return;
  std::fflush(g_file);
  std::fprintf(g_file, "# total %llu records\n", static_cast<unsigned long long>(g_count));
  std::fflush(g_file);
  std::fclose(g_file);
  g_file = nullptr;
}
