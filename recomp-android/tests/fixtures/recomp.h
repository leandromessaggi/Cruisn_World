#pragma once
/* Minimal C ABI fixture only for ROM-free CI. Private integration tests use
   the real N64Recomp recomp.h. Does not model the entire MIPS context. */
#include <stdint.h>
typedef struct { uint64_t r2, r4, r5, r6; } recomp_context;
