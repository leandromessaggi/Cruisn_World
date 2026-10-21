#pragma once
// CI fixture ONLY: the private original-code test uses N64Recomp's real header.
#include <stdint.h>
typedef struct {
  uint64_t r2,r4,r5,r6,r7,r29;
} recomp_context;
