#include <string.h>
#include "unity.h"
#include "../display/main/photo_bmp.h"

static uint8_t header[54];
static photo_bmp_t bmp;
static void put32(int at, uint32_t v)
{
    for (int i = 0; i < 4; i++) header[at + i] = (uint8_t)(v >> (i * 8));
}
void setUp(void)
{
    memset(header, 0, sizeof header);
    header[0] = 'B'; header[1] = 'M'; header[26] = 1; header[28] = 24;
    put32(2, 1152054); put32(10, 54); put32(14, 40); put32(18, 800); put32(22, 0u - 480u);
}
void tearDown(void) {}

static void test_top_down_screenshot(void)
{
    TEST_ASSERT_TRUE(photo_bmp_header(header, sizeof header, &bmp));
    TEST_ASSERT_EQUAL_UINT32(800, bmp.width); TEST_ASSERT_EQUAL_UINT32(480, bmp.height);
    TEST_ASSERT_EQUAL_UINT32(2400, bmp.row_bytes); TEST_ASSERT_TRUE(bmp.top_down);
}
static void test_bottom_up_and_padded_rows(void)
{
    put32(18, 3); put32(22, 2); put32(2, 78);
    TEST_ASSERT_TRUE(photo_bmp_header(header, sizeof header, &bmp));
    TEST_ASSERT_EQUAL_UINT32(12, bmp.row_bytes); TEST_ASSERT_FALSE(bmp.top_down);
}
static void test_rejects_truncated_header(void)
{ TEST_ASSERT_FALSE(photo_bmp_header(header, 53, &bmp)); }
static void test_rejects_wrong_format(void)
{
    header[0] = 'X'; TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp)); header[0] = 'B';
    header[28] = 32; TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp)); header[28] = 24;
    put32(30, 1); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp)); put32(30, 0);
    put32(14, 124); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
}
static void test_rejects_oversized_and_invalid_dimensions(void)
{
    put32(18, 801); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
    put32(18, 0); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
    put32(18, 0u - 800u); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
    put32(18, 800); put32(22, 481); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
    put32(22, 0x80000000u); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
    put32(22, 0); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
}
static void test_rejects_bad_offset_and_length(void)
{
    put32(10, 53); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
    put32(10, 1025); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
    put32(10, 54); put32(2, 1152053); TEST_ASSERT_FALSE(photo_bmp_header(header, 54, &bmp));
}
static void test_bgr_to_rgb565(void)
{
    uint8_t red[] = {0, 0, 255}, green[] = {0, 255, 0}, blue[] = {255, 0, 0}, white[] = {255, 255, 255};
    TEST_ASSERT_EQUAL_HEX16(0xf800, photo_rgb565(red));
    TEST_ASSERT_EQUAL_HEX16(0x07e0, photo_rgb565(green));
    TEST_ASSERT_EQUAL_HEX16(0x001f, photo_rgb565(blue));
    TEST_ASSERT_EQUAL_HEX16(0xffff, photo_rgb565(white));
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_top_down_screenshot); RUN_TEST(test_bottom_up_and_padded_rows);
    RUN_TEST(test_rejects_truncated_header); RUN_TEST(test_rejects_wrong_format);
    RUN_TEST(test_rejects_oversized_and_invalid_dimensions); RUN_TEST(test_rejects_bad_offset_and_length);
    RUN_TEST(test_bgr_to_rgb565);
    return UNITY_END();
}
