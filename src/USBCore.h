#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "usb_ch9.h"
#include <stdbool.h>

#include "MAX3421E.h"
#include "MAX3421E_registers.h"
#include "PlatformConfig.h"
#include "USBDevice.h"
#include "UsbDescriptorParser.h"
#include "log.h"
#include "types_shortcuts.h"

// HRSL Bits
#define bmRCVTOGRD 0x10
#define bmSNDTOGRD 0x20
#define bmKSTATUS 0x40
#define bmJSTATUS 0x80
#define bmSE0 0x00 //SE0 - disconnect state
#define bmSE1 0xc0 //SE1 - illegal state
#define SE0 0
#define SE1 1
//#define FSHOST  = 2
//#define LSHOST  = 3

// Set transfer bounds
#define NAK_LIMIT 10
#define RETRY_LIMIT 3

#define LSHOST bmKSTATUS
#define FSHOST bmJSTATUS

typedef struct {
    union {
        //0 Bit-map of request type
        uint8_t bmRequestType;
        struct {
            //Recipient of the request
            uint8_t recipient : 5;
            //Type of request
            uint8_t type : 2;
            //Direction of data X-fer
            uint8_t direction : 1;
        } __attribute__((packed));
    } ReqType_u;

    //1 Request
    uint8_t bRequest;
    union {
        //   2  Depends on bRequest
        uint16_t wValue;
        struct {
            uint8_t wValueLo;
            uint8_t wValueHi;
        } __attribute__((packed));
    } wVal_u;
    //4 Depends on bRequest
    uint16_t wIndex;
    //6 Depends on bRequest
    uint16_t wLength;
} __attribute__((packed)) SETUP_PKT, *PSETUP_PKT;

#define bmREQ_GET_DESCR USB_SETUP_DEVICE_TO_HOST | USB_SETUP_TYPE_STANDARD | USB_SETUP_RECIPIENT_DEVICE //get descriptor request type
#define bmREQ_SET USB_SETUP_HOST_TO_DEVICE | USB_SETUP_TYPE_STANDARD | USB_SETUP_RECIPIENT_DEVICE //set request type for all but 'set feature' and 'set interface'
#define bmREQ_CL_GET_INTF USB_SETUP_DEVICE_TO_HOST | USB_SETUP_TYPE_CLASS | USB_SETUP_RECIPIENT_INTERFACE //get interface request type

// Macros
#define P_SETBIT(reg, val) Pwreg(reg, (Prreg(reg) | val));
#define P_CLRBIT(reg, val) Pwreg(reg, (Prreg(reg) & ~val));
#define P_STALL_EP0 Pwreg(rEPSTALLS, 0x23); // Set all three EP0 stall bits--data stage IN/OUT and status stage

//forward declaration
typedef struct USBDevice USBDevice;

struct usbcore {
    void (*init)(USBDevice*, PlatformConfig*);
    void (*poll)(void);
    u8 (*in_transfer)(u8 endpoint, u16 INbytes);
    u8* (*get_usb_buffer)(void);
    u8 (*send_packet)(u8 token, u8 endpoint);
    u8 (*control_write_no_data)(u8 bmRequestType, u8 bRequest, u8 wValueLo, u8 wValueHi, u16 wIndex, u16 wLength);
    u8 (*control_read_transfer)(u8 bmRequestType, u8 bRequest, u8 wValueLo, u8 wValueHi, u16 wIndex, u16 wLength);
    u32* (*get_last_transfer_size)(void);
};

extern const struct usbcore USBCore;
