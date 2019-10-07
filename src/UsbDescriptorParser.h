#pragma once

#include "PlatformConfig.h"
#include "USBCore.h"
#include "USBDevice.h"
#include "log.h"
#include "types_shortcuts.h"

//forward declaration
typedef struct USBDevice USBDevice;

struct usbdescriptorparser {
    void (*init)(PlatformConfig*);
    void (*peek_device_descriptor)(USBDevice* my_usb_device);
    void (*parse_device_descriptor)(USBDevice* my_usb_device);
    void (*parse_config_descriptor)(USBDevice* my_usb_device);
};

extern const struct usbdescriptorparser UsbDescriptorParser;