/* Display deadlines only: this clock never advances the guest's timers. */
#pragma once
#include <stdint.h>

typedef struct hle_frame_pacer {
    uint64_t frequency, next;
    unsigned hz, remainder;
    int started;
} hle_frame_pacer;

static inline void hle_frame_pacer_reset(hle_frame_pacer *p, uint64_t frequency, unsigned hz) {
    *p = (hle_frame_pacer){ .frequency = frequency, .hz = hz };
}

/* An absolute, fractional deadline avoids both integer-millisecond limits
 * on high-refresh displays and a burst of presentations after a stall. */
static inline int hle_frame_pacer_due(hle_frame_pacer *p, uint64_t now) {
    if (!p->hz || !p->frequency) return 1;
    if (p->started && now < p->next) return 0;
    if (!p->started || now - p->next > p->frequency) {
        p->next = now;
        p->remainder = 0;
        p->started = 1;
    }
    do {
        p->next += p->frequency / p->hz;
        p->remainder += (unsigned)(p->frequency % p->hz);
        p->next += p->remainder / p->hz;
        p->remainder %= p->hz;
    } while (p->next <= now);
    return 1;
}
