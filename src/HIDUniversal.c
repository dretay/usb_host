#include "HIDUniversal.h"

static PlatformConfig platform_config;

static USBDevice device;
static uint8_t prevBuf[constBuffLen];
void (*handler)(uint8_t* prev_buf, uint8_t* buf, u16 read);

static void zero_memory(uint8_t len, uint8_t* buf)
{
    for (uint8_t i = 0; i < len; i++)
        buf[i] = 0;
}
static bool buffers_identical(uint8_t len, uint8_t* buf1, uint8_t* buf2)
{
    for (uint8_t i = 0; i < len; i++)
        if (buf1[i] != buf2[i])
            return false;
    return true;
}
static void save_buffer(uint8_t len, uint8_t* src, uint8_t* dest)
{
    for (uint8_t i = 0; i < len; i++)
        dest[i] = src[i];
}
static void parse()
{
}

static u8 poll()
{
    uint8_t rcode = 0;
    if (!device.poll_enabled) {
        return rcode;
    }
    if ((int32_t)(platform_config.millis() - device.next_poll_time) >= 0L) {
        device.next_poll_time = platform_config.millis() + device.endpoint_descriptor.bInterval;
        uint8_t buf[constBuffLen];
        uint16_t read = device.endpoint_descriptor.wMaxPacketSize;
        zero_memory(constBuffLen, buf);

        rcode = USBCore.in_transfer(device.endpoint_descriptor.bEndpointAddress, read);

        if (rcode) {
            if (rcode != hrNAK)
                //USBTRACE3("(hiduniversal.h) Poll:", rcode, 0x81);
                return rcode;
        }
        memcpy(buf, USBCore.get_usb_buffer(), constBuffLen);

        if (read > constBuffLen)
            read = constBuffLen;

        bool identical = buffers_identical(read, buf, prevBuf);

        if (!identical && handler != NULL) {
            log_trace("Non-identical input report: ");
            for (int ix = 0; ix < read; ix++) {
                log_append("%02X ", buf[ix]);
            }

            handler(prevBuf, buf, read);
        }
        save_buffer(read, buf, prevBuf);
    }
    return rcode;
}
u32 get_report_descriptor(char* output, u32 size)
{
    // Get the 9-byte configuration descriptor

    log_info("HID Configuration Descriptor ");
    log_info("\t");
    u32* buffer_size = USBCore.get_last_transfer_size();
    u8* usb_buffer = USBCore.get_usb_buffer();
    if (!USBCore.control_read_transfer(0x81, USB_REQUEST_GET_DESCRIPTOR, 0, HID_REPORT_DESCRIPTOR, 0, 141)) {
        int output_idx = 0;
        for (int i = 0; i < *buffer_size; i++) {
            output[output_idx] = usb_buffer[i];
            log_append("%02X ", usb_buffer[i]);
            if ((i + 1) % 8 == 0) {
                log_info("\t");
            }
            output_idx++;
            if (output_idx >= size) {
                break;
            }
        }
        return *buffer_size;
    }
    return 0;
}
static void configure(void)
{
    MAX3421E.write_register(rHCTL, bmRCVTOG0);
    //configuration = 1
    USBCore.control_write_no_data(bmREQ_SET, USB_REQUEST_SET_CONFIGURATION, 1, 0, 0, 0);

    //duration=indefinite report=0
    USBCore.control_write_no_data(0x21, USB_REQUEST_GET_INTERFACE, 0, 0, 0, 0);

    device.poll_enabled = true;
}
static void set_handler(void (*handler_in)(uint8_t* prev_buf, uint8_t* buf, u16 read))
{
    handler = handler_in;
}
static USBDevice* new (PlatformConfig* platform_config_in)
{
    memcpy(&platform_config, platform_config_in, sizeof(PlatformConfig));
    device.poll_enabled = false;
    device.next_poll_time = 0;
    device.poll = poll;
    device.parse = parse;
    device.configure = configure;
    device.set_handler = set_handler;
    return &device;
}
const struct hiduniversal HIDUniversal = {
    .new = new,
    .get_report_descriptor = get_report_descriptor,
};
