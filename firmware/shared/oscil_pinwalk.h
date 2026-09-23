#pragma once
typedef struct {
    int gpio;
    const char *name;
} oscil_pin_t;

void test_pinwalk(const oscil_pin_t *pins, int count);