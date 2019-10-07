#pragma once

#include "MAX3421E_registers.h"
#include "PlatformConfig.h"
#include "types_shortcuts.h"
#include <string.h>

#define VBUS_ON write_register(rIOPINS2, (read_register(rIOPINS2) | 0x08));
#define VBUS_OFF write_register(rIOPINS2, (read_register(rIOPINS2) & ~0x08));

struct max3421e {
    void (*init)(PlatformConfig*);
    void (*write_register)(u8 reg, u8 val);
    u8 (*read_register)(u8 reg);
    void (*read_bytes)(u8 reg, u8 bytes, u8* p);
    void (*write_bytes)(u8 reg, u8 N, u8* p);
    void (*soft_reset)(void);
    void (*hard_reset)(void);
    void (*clear_conn_detect_irq)(void);
    void (*enable_irq)(void);
    void (*reset_bus)(void);
};

extern const struct max3421e MAX3421E;