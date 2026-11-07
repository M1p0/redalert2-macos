#include <stdint.h>
#include <stdio.h>
#include "win32hle.h"
#include "frame_pacer.h"

const recomp_dispatch_entry_t recomp_dispatch_table[] = { { 0, 0 } };
const uint32_t recomp_dispatch_count = 0;

int main(void) {
    int failed = 0;
    const unsigned rates[] = { 30, 60, 120, 144, 240 };
    for (unsigned i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        hle_frame_pacer p;
        hle_frame_pacer_reset(&p, 1000000, rates[i]);
        unsigned frames = 0;
        for (uint64_t t = 0; t < 1000000; t++) frames += hle_frame_pacer_due(&p, t);
        if (frames != rates[i]) { fprintf(stderr, "%u Hz: got %u frames\n", rates[i], frames); failed++; }
        if (!hle_frame_pacer_due(&p, 10000000) || hle_frame_pacer_due(&p, 10000000)) {
            fprintf(stderr, "%u Hz: stalled display caught up with a burst\n", rates[i]); failed++;
        }
        hle_frame_pacer_reset(&p, 1000000, 60);
        if (!hle_frame_pacer_due(&p, 10000000) || hle_frame_pacer_due(&p, 10000001)) failed++;
    }
    printf("frame_pacer_selftest: %s\n", failed ? "FAILED" : "all checks passed");
    return failed != 0;
}
