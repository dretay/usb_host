#pragma once

#include "types_shortcuts.h"
#include <stdbool.h>
#include <stdlib.h>

typedef struct PlatformConfig {
    void (*spi_transmit)(u8* out, size_t out_size);
    void (*spi_transmit_receive)(u8* out, size_t out_size, u8* in, size_t in_size);
    void (*hard_reset)(void);
    void (*delay)(int ms);
    u32 (*millis)(void);

    void (*log_log)(bool append, bool terminate, int level, const char* file, const char* function, int line, const char* fmt, ...);

} PlatformConfig;
