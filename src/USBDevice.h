#pragma once

#include "USBCore.h"
#include "UsbDescriptorParser.h"
#include "stdbool.h"
#include "types_shortcuts.h"
#include "usb_ch9.h"

struct USBDevice {
    u8 (*poll)(void);
    void (*parse)(void);
    void (*configure)(void);
    bool (*get_device_descriptor)(u8 length);
    void (*set_handler)(void (*handler)(uint8_t* prev_buf, uint8_t* buf));
    u8 address;
    USB_CONFIGURATION_DESCRIPTOR configuration_descriptor;
    USB_INTERFACE_DESCRIPTOR interface_descriptor;
    USB_HID_DESCRIPTOR hid_descriptor;
    USB_ENDPOINT_DESCRIPTOR endpoint_descriptor;
    USB_DEVICE_DESCRIPTOR device_descriptor;
    bool poll_enabled;
    u32 next_poll_time;
};
