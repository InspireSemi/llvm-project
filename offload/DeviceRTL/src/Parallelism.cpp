//===---- Parallelism.cpp - OpenMP GPU parallel implementation ---- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Parallel implementation in the GPU. Here is the pattern:
//
//    while (not finished) {
//
//    if (master) {
//      sequential code, decide which par loop to do, or if finished
//     __kmpc_kernel_prepare_parallel() // exec by master only
//    }
//    syncthreads // A
//    __kmpc_kernel_parallel() // exec by all
//    if (this thread is included in the parallel) {
//      switch () for all parallel loops
//      __kmpc_kernel_end_parallel() // exec only by threads in parallel
//    }
//
//
//    The reason we don't exec end_parallel for the threads not included
//    in the parallel loop is that for each barrier in the parallel
//    region, these non-included threads will cycle through the
//    syncthread A. Thus they must preserve their current threadId that
//    is larger than thread in team.
//
//    To make a long story short...
//
//===----------------------------------------------------------------------===//

#include "Debug.h"
#include "DeviceTypes.h"
#include "DeviceUtils.h"
#include "Interface.h"
#include "LibC.h"
#include "Mapping.h"
#include "State.h"
#include "Synchronization.h"

#ifdef OMPTARGET_DEVICE_THUNDERBIRD
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <stdint.h>

// Thread-local state for Thunderbird
// Must use global-dynamic TLS model for shared libraries loaded via dlopen().
// The tls_model attribute is required because LLVM's TargetMachine::getTLSModel()
// incorrectly chooses local-exec for static TLS variables even when building with
// -shared, causing R_RISCV_TPREL_* relocations that are incompatible with shared
// libraries. The attribute embeds the TLS model in the IR metadata, ensuring it
// survives bitcode linking and overrides the backend's default selection logic.
static __thread uint32_t tbird_thread_id __attribute__((tls_model("global-dynamic"))) = 0;
static __thread uint32_t tbird_team_size __attribute__((tls_model("global-dynamic"))) = 1;

extern "C" {
  uint32_t __tbird_get_thread_id(void) { return tbird_thread_id; }
  uint32_t __tbird_get_team_size(void) { return tbird_team_size; }
}

// Configuration-gated diagnostics for the Thunderbird parallelism/affinity path
// (fork/thread lifecycle prints + the hart-placement probe). Compiled out by
// default; define TBIRD_DEVICE_TRACE at DeviceRTL build time (e.g. add
// -DTBIRD_DEVICE_TRACE to the offload-runtime recipe's DeviceRTL flags) to
// include them. Default builds carry no prints or format strings — zero runtime
// cost, and production kernels stay lean.
#ifdef TBIRD_DEVICE_TRACE
#define TBIRD_TRACE(...) printf(__VA_ARGS__)
#else
#define TBIRD_TRACE(...) ((void)0)
#endif

#endif

using namespace ompx;

namespace {

uint32_t determineNumberOfThreads(int32_t NumThreadsClause) {
  uint32_t NThreadsICV =
      NumThreadsClause != -1 ? NumThreadsClause : icv::NThreads;
  uint32_t NumThreads = mapping::getMaxTeamThreads();

  if (NThreadsICV != 0 && NThreadsICV < NumThreads)
    NumThreads = NThreadsICV;

  // SPMD mode allows any number of threads, for generic mode we round down to a
  // multiple of WARPSIZE since it is legal to do so in OpenMP.
  if (mapping::isSPMDMode())
    return NumThreads;

  if (NumThreads < mapping::getWarpSize())
    NumThreads = 1;
  else
    NumThreads = (NumThreads & ~((uint32_t)mapping::getWarpSize() - 1));

  return NumThreads;
}

// Invoke an outlined parallel function unwrapping arguments (up to 32).
[[clang::always_inline]] void invokeMicrotask(int32_t global_tid,
                                              int32_t bound_tid, void *fn,
                                              void **args, int64_t nargs) {
  switch (nargs) {
#include "generated_microtask_cases.gen"
  default:
    printf("Too many arguments in kmp_invoke_microtask, aborting execution.\n");
    __builtin_trap();
  }
}

} // namespace

extern "C" {

[[clang::always_inline]] void __kmpc_parallel_spmd(IdentTy *ident,
                                                   int32_t num_threads,
                                                   void *fn, void **args,
                                                   const int64_t nargs) {
  uint32_t TId = mapping::getThreadIdInBlock();
  uint32_t NumThreads = determineNumberOfThreads(num_threads);
  uint32_t PTeamSize =
      NumThreads == mapping::getMaxTeamThreads() ? 0 : NumThreads;
  // Avoid the race between the read of the `icv::Level` above and the write
  // below by synchronizing all threads here.
  synchronize::threadsAligned(atomic::seq_cst);
  {
    // Note that the order here is important. `icv::Level` has to be updated
    // last or the other updates will cause a thread specific state to be
    // created.
    state::ValueRAII ParallelTeamSizeRAII(state::ParallelTeamSize, PTeamSize,
                                          1u, TId == 0, ident,
                                          /*ForceTeamState=*/true);
    state::ValueRAII ActiveLevelRAII(icv::ActiveLevel, 1u, 0u, TId == 0, ident,
                                     /*ForceTeamState=*/true);
    state::ValueRAII LevelRAII(icv::Level, 1u, 0u, TId == 0, ident,
                               /*ForceTeamState=*/true);

    // Synchronize all threads after the main thread (TId == 0) set up the
    // team state properly.
    synchronize::threadsAligned(atomic::acq_rel);

    state::ParallelTeamSize.assert_eq(PTeamSize, ident,
                                      /*ForceTeamState=*/true);
    icv::ActiveLevel.assert_eq(1u, ident, /*ForceTeamState=*/true);
    icv::Level.assert_eq(1u, ident, /*ForceTeamState=*/true);

    // Ensure we synchronize before we run user code to avoid invalidating the
    // assumptions above.
    synchronize::threadsAligned(atomic::relaxed);

    if (!PTeamSize || TId < PTeamSize)
      invokeMicrotask(TId, 0, fn, args, nargs);

    // Synchronize all threads at the end of a parallel region.
    synchronize::threadsAligned(atomic::seq_cst);
  }

  // Synchronize all threads to make sure every thread exits the scope above;
  // otherwise the following assertions and the assumption in
  // __kmpc_target_deinit may not hold.
  synchronize::threadsAligned(atomic::acq_rel);

  state::ParallelTeamSize.assert_eq(1u, ident, /*ForceTeamState=*/true);
  icv::ActiveLevel.assert_eq(0u, ident, /*ForceTeamState=*/true);
  icv::Level.assert_eq(0u, ident, /*ForceTeamState=*/true);

  // Ensure we synchronize to create an aligned region around the assumptions.
  synchronize::threadsAligned(atomic::relaxed);

  return;
}

[[clang::always_inline]] void
__kmpc_parallel_51(IdentTy *ident, int32_t, int32_t if_expr,
                   int32_t num_threads, int proc_bind, void *fn,
                   void *wrapper_fn, void **args, int64_t nargs) {
  uint32_t TId = mapping::getThreadIdInBlock();

  // Assert the parallelism level is zero if disabled by the user.
  ASSERT((config::mayUseNestedParallelism() || icv::Level == 0),
         "nested parallelism while disabled");

  // Handle the serialized case first, same for SPMD/non-SPMD:
  // 1) if-clause(0)
  // 2) parallel in task or other thread state inducing construct
  // 3) nested parallel regions
  if (OMP_UNLIKELY(!if_expr || state::HasThreadState ||
                   (config::mayUseNestedParallelism() && icv::Level))) {
    state::DateEnvironmentRAII DERAII(ident);
    ++icv::Level;
    invokeMicrotask(TId, 0, fn, args, nargs);
    return;
  }

  // From this point forward we know that there is no thread state used.
  ASSERT(state::HasThreadState == false, nullptr);

  if (mapping::isSPMDMode()) {
    // This was moved to its own routine so it could be called directly
    // in certain situations to avoid resource consumption of unused
    // logic in parallel_51.
    __kmpc_parallel_spmd(ident, num_threads, fn, args, nargs);

    return;
  }

  uint32_t NumThreads = determineNumberOfThreads(num_threads);
  uint32_t MaxTeamThreads = mapping::getMaxTeamThreads();
  uint32_t PTeamSize = NumThreads == MaxTeamThreads ? 0 : NumThreads;

  // We do *not* create a new data environment because all threads in the team
  // that are active are now running this parallel region. They share the
  // TeamState, which has an increase level-var and potentially active-level
  // set, but they do not have individual ThreadStates yet. If they ever
  // modify the ICVs beyond this point a ThreadStates will be allocated.

  bool IsActiveParallelRegion = NumThreads > 1;
  if (!IsActiveParallelRegion) {
    state::ValueRAII LevelRAII(icv::Level, 1u, 0u, true, ident);
    invokeMicrotask(TId, 0, fn, args, nargs);
    return;
  }

  void **GlobalArgs = nullptr;
  if (nargs) {
    __kmpc_begin_sharing_variables(&GlobalArgs, nargs);
    switch (nargs) {
    default:
      for (int I = 0; I < nargs; I++)
        GlobalArgs[I] = args[I];
      break;
    case 16:
      GlobalArgs[15] = args[15];
      [[fallthrough]];
    case 15:
      GlobalArgs[14] = args[14];
      [[fallthrough]];
    case 14:
      GlobalArgs[13] = args[13];
      [[fallthrough]];
    case 13:
      GlobalArgs[12] = args[12];
      [[fallthrough]];
    case 12:
      GlobalArgs[11] = args[11];
      [[fallthrough]];
    case 11:
      GlobalArgs[10] = args[10];
      [[fallthrough]];
    case 10:
      GlobalArgs[9] = args[9];
      [[fallthrough]];
    case 9:
      GlobalArgs[8] = args[8];
      [[fallthrough]];
    case 8:
      GlobalArgs[7] = args[7];
      [[fallthrough]];
    case 7:
      GlobalArgs[6] = args[6];
      [[fallthrough]];
    case 6:
      GlobalArgs[5] = args[5];
      [[fallthrough]];
    case 5:
      GlobalArgs[4] = args[4];
      [[fallthrough]];
    case 4:
      GlobalArgs[3] = args[3];
      [[fallthrough]];
    case 3:
      GlobalArgs[2] = args[2];
      [[fallthrough]];
    case 2:
      GlobalArgs[1] = args[1];
      [[fallthrough]];
    case 1:
      GlobalArgs[0] = args[0];
      [[fallthrough]];
    case 0:
      break;
    }
  }

  {
    // Note that the order here is important. `icv::Level` has to be updated
    // last or the other updates will cause a thread specific state to be
    // created.
    state::ValueRAII ParallelTeamSizeRAII(state::ParallelTeamSize, PTeamSize,
                                          1u, true, ident,
                                          /*ForceTeamState=*/true);
    state::ValueRAII ParallelRegionFnRAII(state::ParallelRegionFn, wrapper_fn,
                                          (void *)nullptr, true, ident,
                                          /*ForceTeamState=*/true);
    state::ValueRAII ActiveLevelRAII(icv::ActiveLevel, 1u, 0u, true, ident,
                                     /*ForceTeamState=*/true);
    state::ValueRAII LevelRAII(icv::Level, 1u, 0u, true, ident,
                               /*ForceTeamState=*/true);

    // Master signals work to activate workers.
    synchronize::threads(atomic::seq_cst);
    // Master waits for workers to signal.
    synchronize::threads(atomic::seq_cst);
  }

  if (nargs)
    __kmpc_end_sharing_variables();
}

[[clang::noinline]] bool __kmpc_kernel_parallel(ParallelRegionFnTy *WorkFn) {
  // Work function and arguments for L1 parallel region.
  *WorkFn = state::ParallelRegionFn;

  // If this is the termination signal from the master, quit early.
  if (!*WorkFn)
    return false;

  // Set to true for workers participating in the parallel region.
  uint32_t TId = mapping::getThreadIdInBlock();
  bool ThreadIsActive = TId < state::getEffectivePTeamSize();
  return ThreadIsActive;
}

[[clang::noinline]] void __kmpc_kernel_end_parallel() {
  // In case we have modified an ICV for this thread before a ThreadState was
  // created. We drop it now to not contaminate the next parallel region.
  ASSERT(!mapping::isSPMDMode(), nullptr);
  uint32_t TId = mapping::getThreadIdInBlock();
  state::resetStateForThread(TId);
  ASSERT(!mapping::isSPMDMode(), nullptr);
}

uint16_t __kmpc_parallel_level(IdentTy *, uint32_t) { return omp_get_level(); }

#ifndef OMPTARGET_DEVICE_THUNDERBIRD
// GPU targets use generic thread numbering
int32_t __kmpc_global_thread_num(IdentTy *) { return omp_get_thread_num(); }
#endif

void __kmpc_push_num_teams(IdentTy *loc, int32_t tid, int32_t num_teams,
                           int32_t thread_limit) {}

void __kmpc_push_proc_bind(IdentTy *loc, uint32_t tid, int proc_bind) {}

#ifdef OMPTARGET_DEVICE_THUNDERBIRD
// Linux/pthread-based parallelism for CPU accelerators

// --- Hart-affinity probe (temporary instrumentation, 2026-06-23) -------------
// Per OpenMP team thread, report the hart it is currently running on and the
// size of its CPU-affinity mask. This answers whether a forked team spreads
// across harts or collapses onto the mailbox worker's single pinned vCPU:
// allowed_cpus==1 for every thread means the team inherited the worker's
// one-vCPU mask and is confined to one hart; allowed_cpus==N (and differing
// cpu= values) means it is free to spread. Declared directly to avoid a
// the glibc <sched.h> API (cpu_set_t / CPU_COUNT), which the device sysroot
// provides with __USE_GNU enabled.

namespace {
constexpr uint32_t MaxThunderbirdThreads = 64;

void tbird_hart_probe(const char *who, uint32_t tid) {
#ifdef TBIRD_DEVICE_TRACE
  cpu_set_t set;
  CPU_ZERO(&set);
  int allowed = -1;
  if (sched_getaffinity(0, sizeof(set), &set) == 0)
    allowed = CPU_COUNT(&set);
  int cpu = sched_getcpu();
  printf("[DeviceRTL:hartprobe] %s tid=%u cpu=%d allowed_cpus=%d\n", who, tid,
         cpu, allowed);
#else
  (void)who;
  (void)tid;
#endif
}

// Layer-1 hart-spread fix (2026-06-23): build an affinity mask over all online
// harts so a forked team can use the whole device, instead of inheriting the
// mailbox worker's single pinned vCPU (which confines the entire team to one
// hart). This must be set affirmatively: the device's default process affinity
// is itself a single hart, so merely declining to pin is not enough. Uses
// get_nprocs_conf() (configured CPU count) rather than sysconf/nproc, which are
// affinity-limited and would under-report here. Online harts are 0..N-1.
void tbird_build_online_mask(cpu_set_t *set) {
  CPU_ZERO(set);
  int n = get_nprocs_conf();
  if (n < 1)
    n = 1;
  if (n > CPU_SETSIZE)
    n = CPU_SETSIZE;
  for (int i = 0; i < n; ++i)
    CPU_SET(i, set);
}

struct ThreadPayload {
  void *Microtask;
  void **Args;
  int64_t NArgs;
  int32_t GlobalTid;
  uint32_t TeamSize;
};

void *threadEntry(void *arg) {
  auto *payload = static_cast<ThreadPayload *>(arg);
  
  // Set thread-local state
  tbird_thread_id = static_cast<uint32_t>(payload->GlobalTid);
  tbird_team_size = payload->TeamSize;
  
  TBIRD_TRACE("[DeviceRTL:threadEntry] Worker thread %u starting (team_size=%u)\n",
         tbird_thread_id, tbird_team_size);
  tbird_hart_probe("worker", tbird_thread_id);

  // Invoke the microtask
  int32_t gtid = payload->GlobalTid;
  int32_t btid = 0;  // bound tid (unused in OpenMP)
  
  invokeMicrotask(gtid, btid, payload->Microtask, payload->Args, payload->NArgs);
  
  return nullptr;
}

} // namespace

void __kmpc_push_num_threads(IdentTy *loc, int32_t global_tid,
                             int32_t num_threads) {
  // Store the number of threads to use in the next parallel region
  if (num_threads > 0)
    icv::NThreads = num_threads;
}

void __kmpc_fork_call(IdentTy *loc, int32_t argc, void *microtask, ...) {
  // Extract variadic arguments
  void *args[MaxThunderbirdThreads];
  if (argc > static_cast<int32_t>(MaxThunderbirdThreads)) {
    printf("__kmpc_fork_call: too many arguments (%d), max is %u\n",
           argc, MaxThunderbirdThreads);
    __builtin_trap();
  }
  
  va_list ap;
  va_start(ap, microtask);
  for (int i = 0; i < argc; i++) {
    args[i] = va_arg(ap, void *);
  }
  va_end(ap);
  
  // Determine number of threads
  uint32_t requested = (icv::NThreads > 0) ? icv::NThreads : 1;
  uint32_t num_threads = requested;
  if (num_threads > MaxThunderbirdThreads)
    num_threads = MaxThunderbirdThreads;
  
  TBIRD_TRACE("[DeviceRTL:fork_call] requested=%u, num_threads=%u\n", requested, num_threads);
  
  // Serial execution
  if (num_threads == 1) {
    tbird_team_size = 1;
    tbird_thread_id = 0;
    
    TBIRD_TRACE("[DeviceRTL:fork_call] Serial execution: tid=%u, team_size=%u\n",
           tbird_thread_id, tbird_team_size);
    
    int32_t gtid = 0, btid = 0;
    invokeMicrotask(gtid, btid, microtask, args, argc);
    
    icv::NThreads = 0;
    return;
  }
  
  // Parallel execution with pthreads
  tbird_team_size = num_threads;

  TBIRD_TRACE("[DeviceRTL:fork_call] Parallel execution: team_size=%u\n", num_threads);

  // Layer-1 hart-spread: create the team over all online harts rather than
  // letting the workers inherit the mailbox worker's single-vCPU pin. Worker
  // threads get the full mask via a pthread_attr; the master (this thread) is
  // broadened for the region and restored after the join so the change stays
  // scoped to the parallel region.
  cpu_set_t full_mask;
  tbird_build_online_mask(&full_mask);
  pthread_attr_t spread_attr;
  pthread_attr_init(&spread_attr);
  pthread_attr_setaffinity_np(&spread_attr, sizeof(full_mask), &full_mask);
  cpu_set_t master_orig;
  bool master_saved =
      (pthread_getaffinity_np(pthread_self(), sizeof(master_orig),
                              &master_orig) == 0);
  pthread_setaffinity_np(pthread_self(), sizeof(full_mask), &full_mask);

  pthread_t threads[MaxThunderbirdThreads];
  ThreadPayload payloads[MaxThunderbirdThreads];
  bool created[MaxThunderbirdThreads] = {false};
  
  // Prepare payloads for all threads
  for (uint32_t i = 0; i < num_threads; ++i) {
    payloads[i].Microtask = microtask;
    payloads[i].Args = args;
    payloads[i].NArgs = argc;
    payloads[i].GlobalTid = static_cast<int32_t>(i);
    payloads[i].TeamSize = num_threads;
  }
  
  // Launch worker threads (1..N-1)
  for (uint32_t i = 1; i < num_threads; ++i) {
    TBIRD_TRACE("[DeviceRTL:fork_call] Creating thread %u\n", i);
    if (pthread_create(&threads[i], &spread_attr, threadEntry, &payloads[i]) == 0) {
      created[i] = true;
      TBIRD_TRACE("[DeviceRTL:fork_call] Thread %u created successfully\n", i);
    } else {
      TBIRD_TRACE("[DeviceRTL:fork_call] Thread %u creation FAILED\n", i);
      // pthread_create failed - execute serially on master
      tbird_thread_id = i;
      int32_t gtid = static_cast<int32_t>(i);
      int32_t btid = 0;
      invokeMicrotask(gtid, btid, microtask, args, argc);
    }
  }
  
  // Master thread executes as thread 0
  tbird_thread_id = 0;
  TBIRD_TRACE("[DeviceRTL:fork_call] Master thread executing as tid=%u, team_size=%u\n",
         tbird_thread_id, tbird_team_size);
  tbird_hart_probe("master", tbird_thread_id);
  int32_t gtid = 0, btid = 0;
  invokeMicrotask(gtid, btid, microtask, args, argc);
  
  // Join worker threads
  for (uint32_t i = 1; i < num_threads; ++i) {
    if (created[i]) {
      pthread_join(threads[i], nullptr);
    }
  }

  // Restore the master/worker thread's original affinity (scope the broadening
  // to this region) and release the spread attribute.
  if (master_saved)
    pthread_setaffinity_np(pthread_self(), sizeof(master_orig), &master_orig);
  pthread_attr_destroy(&spread_attr);

  // Reset state
  icv::NThreads = 0;
  tbird_team_size = 1;
  tbird_thread_id = 0;
}

int32_t __kmpc_global_thread_num(IdentTy *) {
  return static_cast<int32_t>(tbird_thread_id);
}

#endif // OMPTARGET_DEVICE_THUNDERBIRD

}
