#pragma once

// Function-call trace hooks for the -finstrument-functions build (build_trace.cmd).
// Every instrumented function's entry/exit lands here when CROSSPOINT_TRACE=1 is
// set; the hook writes "E <ms> <addr>" / "X <ms> <addr>" records to trace.log.
// Resolve addresses and build the call tree with scripts/trace_analyze.py.
//
// This file is compiled WITHOUT instrumentation (see CMakeLists.txt), and the
// hooks use CRT/SDL calls only, so a trace cannot recurse into itself.

void sim_trace_init(void);
void sim_trace_flush(void);
