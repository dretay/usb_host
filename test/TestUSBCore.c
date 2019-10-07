#include "../src/HIDUniversal.h"
#include "../src/USBCore.h"
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
void dummy_spi_transmit(u8* out, size_t out_size) {}
void dummy_spi_transmit_receive(u8* out, size_t out_size, u8* in, size_t in_size) {}
void dummy_hard_reset(void) {}
void dummy_delay(int ms) {}

void test_adding_handler(void)
{
    PlatformConfig config;
    config.millis = dummy_millis;
    config.delay = dummy_delay;
    config.spi_transmit = dummy_spi_transmit;
    config.spi_transmit_receive = dummy_spi_transmit_receive;
    config.hard_reset = dummy_hard_reset;

    USBDevice* usb_device = HIDUniversal.new(&config);
    USBCore.init(usb_device, &config);

    // TEST_ASSERT_EQUAL(dummy_millis, USBCore.millis);
}
