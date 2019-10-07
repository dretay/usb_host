#include "../src/HIDUniversal.h"
#include "unity.h"
#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

//general setup / teardown tests
u32 dummy_millis(void) { return 0; }
void test_adding_handler(void)
{
    PlatformConfig config;
    config.millis = dummy_millis;
    USBDevice* usb_device = HIDUniversal.new(&config);
    TEST_ASSERT_EQUAL(dummy_millis, usb_device->millis);
}
