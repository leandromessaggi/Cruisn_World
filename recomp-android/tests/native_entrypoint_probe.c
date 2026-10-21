/* ROM-specific recompiled entrypoint smoke test. No original game bytes here.
 * This probe MUST be linked with funcs_0.c generated privately from the user's ROM.
 * It observes the first indirect guest-PC dispatch and does not simulate gameplay.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp.h"
#include "funcs.h"

#define RDRAM_SIZE (8u * 1024u * 1024u)
#define BSS_START 0x353380u
#define BSS_BYTES 0x94760u
#define MAIN_PC 0x802AD450u
#define STACK_PTR 0x80150400u

static int observed_dispatch = 0;
static int observed_error = 0;
static uint8_t* test_rdram;

static void first_main_dispatch(uint8_t* rdram, recomp_context* ctx) {
    observed_dispatch++;
    if (rdram != test_rdram || (uint32_t)ctx->r29 != STACK_PTR ||
        (uint32_t)ctx->r10 != MAIN_PC || ctx->r9 != 0) {
        observed_error = 1;
    }
}

recomp_func_t* get_function(int32_t vram) {
    if ((uint32_t)vram != MAIN_PC) {
        fprintf(stderr, "Unexpected first guest dispatch: %08x\n", (uint32_t)vram);
        observed_error = 1;
        return first_main_dispatch;
    }
    return first_main_dispatch;
}

int main(void) {
    uint8_t* rdram = malloc(RDRAM_SIZE);
    if (rdram == NULL) return 2;
    memset(rdram, 0xA5, RDRAM_SIZE);
    recomp_context ctx = {0};
    test_rdram = rdram;
    recomp_entrypoint(rdram, &ctx);
    int fail = observed_error || observed_dispatch != 1;
    for (size_t i = BSS_START; i < BSS_START + BSS_BYTES; ++i) {
        if (rdram[i] != 0) {
            fprintf(stderr, "BSS not cleared at physical offset 0x%zx: %02x\n",i,rdram[i]);
            fail = 1;
            break;
        }
    }
    if (rdram[BSS_START - 1] != 0xA5 || rdram[BSS_START + BSS_BYTES] != 0xA5) {
        fprintf(stderr, "BSS cleared outside expected range\n");
        fail = 1;
    }
    printf("%s: authentic guest entrypoint; BSS=%u bytes; initial SP=%08x; next PC=%08x; dispatches=%d\n",
           fail ? "FAIL" : "PASS", BSS_BYTES, STACK_PTR, MAIN_PC, observed_dispatch);
    free(rdram);
    return fail ? 1 : 0;
}
