#include <stdint.h>
#include "unity.h"
#include "oscil_ring.h"

static uint8_t mem[16];
static oscil_ring_t r;

void setUp(void) { r = (oscil_ring_t){ .size = sizeof mem, .buf = mem }; }
void tearDown(void) {}

void test_fills_to_exactly_size_then_refuses(void)
{
    for (int i = 0; i < 16; i++) TEST_ASSERT_TRUE(ring_put(&r, (uint8_t)i));
    TEST_ASSERT_FALSE(ring_put(&r, 99));
    TEST_ASSERT_EQUAL_UINT32(16, ring_used(&r));
    TEST_ASSERT_EQUAL_UINT32(0, ring_free(&r));
}

void test_bytes_come_out_in_order_then_empty(void)
{
    for (int i = 0; i < 10; i++) ring_put(&r, (uint8_t)(i + 1));
    uint8_t b;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_TRUE(ring_get(&r, &b));
        TEST_ASSERT_EQUAL_UINT8(i + 1, b);
    }
    TEST_ASSERT_FALSE(ring_get(&r, &b));
}

void test_indices_wrap_past_2_to_the_32(void)
{
    r.head = r.tail = 0xFFFFFFF0u;                        // 16 puts from here cross the wrap
    uint8_t b;
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 16; i++) TEST_ASSERT_TRUE(ring_put(&r, (uint8_t)(round * 16 + i)));
        TEST_ASSERT_FALSE(ring_put(&r, 0));
        TEST_ASSERT_EQUAL_UINT32(16, ring_used(&r));
        for (int i = 0; i < 16; i++) {
            TEST_ASSERT_TRUE(ring_get(&r, &b));
            TEST_ASSERT_EQUAL_UINT8(round * 16 + i, b);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fills_to_exactly_size_then_refuses);
    RUN_TEST(test_bytes_come_out_in_order_then_empty);
    RUN_TEST(test_indices_wrap_past_2_to_the_32);
    return UNITY_END();
}