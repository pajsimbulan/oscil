#pragma once
// Single-producer single-consumer byte ring, lock-free. Pure C, no ESP-IDF: host-testable.
#include <stdint.h>
#include <stdbool.h>

// size must be a power of two. head and tail count forever and wrap at 2^32;
// (head - tail) is the fill level.
typedef struct {
    uint32_t head;        // written only by the producer
    uint32_t tail;        // written only by the consumer
    uint32_t size;
    uint8_t *buf;
} oscil_ring_t;

#define RING_INLINE static inline __attribute__((always_inline))

RING_INLINE uint32_t ring_used(const oscil_ring_t *r)                // either side
{
    return __atomic_load_n(&r->head, __ATOMIC_ACQUIRE) - __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);
}

RING_INLINE uint32_t ring_free(const oscil_ring_t *r)                // producer side
{
    return r->size - ring_used(r);
}

RING_INLINE bool ring_put(oscil_ring_t *r, uint8_t b)              // producer side
{
    uint32_t h = r->head;
    if (h - __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE) == r->size) return false;   // full
    r->buf[h & (r->size - 1)] = b;
    __atomic_store_n(&r->head, h + 1, __ATOMIC_RELEASE);          // publish after the data
    return true;
}

RING_INLINE bool ring_get(oscil_ring_t *r, uint8_t *b)             // consumer side
{
    uint32_t t = r->tail;
    if (t == __atomic_load_n(&r->head, __ATOMIC_ACQUIRE)) return false;             // empty
    *b = r->buf[t & (r->size - 1)];
    __atomic_store_n(&r->tail, t + 1, __ATOMIC_RELEASE);          // free the slot after reading
    return true;
}