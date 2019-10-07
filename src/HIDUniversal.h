#pragma once

#include "MAX3421E.h"
#include "PlatformConfig.h"
#include "USBCore.h"
#include "USBDevice.h"
#include "log.h"

#define constBuffLen 64 // event buffer length

struct hiduniversal {
    USBDevice* (*new)(PlatformConfig* config);
};

extern const struct hiduniversal HIDUniversal;