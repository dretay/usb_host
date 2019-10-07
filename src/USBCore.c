#include "USBCore.h"

static int probe_bus(void);
static void busprobe(void);
static void enumerate_device(void);
static u8 in_transfer(u8 endpoint, u16 INbytes);
static void wait_frames(u8 num);
static u8 send_packet(u8 token, u8 endpoint);
static u8 print_error(u8 err);
static void initialize_device(void);
static u8 control_write_no_data(u8 bmRequestType, u8 bRequest, u8 wValueLo, u8 wValueHi, u16 wIndex, u16 wLength);
static PlatformConfig platform_config;

// Global variables
static u8 usb_buffer[2000]; // Big array to handle max size descriptor data

static u16 nak_count, IN_nak_count, HS_nak_count;
static u32 last_transfer_size;

static USBDevice* my_usb_device;

static u8* get_usb_buffer(void)
{
    return usb_buffer;
}
static u32* get_last_transfer_size(void)
{
    return &last_transfer_size;
}
static void poll(void)
{
    platform_config.delay(10);
    my_usb_device->poll();
}
static void set_device_address(u8 address)
{
    log_info("Setting address to %d", address);
    // Issue another USB bus reset
    log_warn("Issuing USB bus reset");

    // initiate the 50 msec bus reset
    MAX3421E.write_register(rHCTL, bmBUSRST);

    // Wait for the bus reset to complete
    while (MAX3421E.read_register(rHCTL) & bmBUSRST)
        ;
    wait_frames(200);
    if (!control_write_no_data(bmREQ_SET, USB_REQUEST_SET_ADDRESS, address, 0, 0, 0)) {
        my_usb_device->address = 7;
        // Device gets 2 msec recovery time
        wait_frames(30);
        // now all transfers go to addr 7
        MAX3421E.write_register(rPERADDR, my_usb_device->address);
    }
}
static void init(USBDevice* usbdevice, PlatformConfig* platform_config_in)
{
    memcpy(&platform_config, platform_config_in, sizeof(PlatformConfig));
    my_usb_device = usbdevice;
    MAX3421E.init(platform_config_in);
    busprobe();
    initialize_device();
    UsbDescriptorParser.init(platform_config_in);
    UsbDescriptorParser.peek_device_descriptor(my_usb_device);
    set_device_address(7);
    UsbDescriptorParser.parse_device_descriptor(my_usb_device);
    UsbDescriptorParser.parse_config_descriptor(my_usb_device);
    usbdevice->configure();
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
}

static void busprobe(void)
{
    int busstate = -1;
    // Activate HOST mode & turn on the 15K pulldown resistors on D+ and D-
    // Note--initially set up as a FS host (LOWSPEED=0)
    MAX3421E.write_register(rMODE, (bmDPPULLDN | bmDMPULLDN | bmHOST));

    // clear the connection detect IRQ
    MAX3421E.write_register(rHIRQ, bmCONDETIRQ);

    // See if anything is plugged in. If not, hang until something plugs in
    do {
        // update the JSTATUS and KSTATUS bits
        MAX3421E.write_register(rHCTL, bmSAMPLEBUS);

        // read them
        busstate = MAX3421E.read_register(rHRSL);

        // check for either of them high
        busstate &= (bmJSTATUS | bmKSTATUS);
    } while (busstate == 0);
    // since we're set to FS, J-state means D+ high
    if (busstate == bmJSTATUS) {
        // make the MAX3421E a full speed host
        MAX3421E.write_register(rMODE, (bmDPPULLDN | bmDMPULLDN | bmHOST | bmSOFKAENAB));
        log_info("Full-Speed Device Detected");
    }
    if (busstate == bmKSTATUS) // K-state means D- high
    {
        // make the MAX3421E a low speed host
        MAX3421E.write_register(rMODE, (bmDPPULLDN | bmDMPULLDN | bmHOST | bmLOWSPEED | bmSOFKAENAB));
        log_info("Low-Speed Device Detected");
    }
    MAX3421E.reset_bus();
}

static void initialize_device()
{
    // First request goes to address 0
    my_usb_device->address = 0;
    MAX3421E.write_register(rPERADDR, my_usb_device->address);
}

static u8 control_read_transfer(u8 bmRequestType, u8 bRequest, u8 wValueLo, u8 wValueHi, u16 wIndex, u16 wLength)
{
    SETUP_PKT setup_pkt;
    setup_pkt.ReqType_u.bmRequestType = bmRequestType;
    setup_pkt.bRequest = bRequest;
    setup_pkt.wVal_u.wValueLo = wValueLo;
    setup_pkt.wVal_u.wValueHi = wValueHi;
    setup_pkt.wIndex = wIndex;
    setup_pkt.wLength = wLength;

    u8 resultcode;

    // SETUP packet
    // Load the Setup data FIFO
    MAX3421E.write_bytes(rSUDFIFO, 8, (u8*)&setup_pkt);

    // SETUP packet to EP0
    resultcode = send_packet(tokSETUP, 0);

    // should be 0, indicating ACK. Else return error code.
    if (resultcode) {
        return (resultcode);
    }

    // One or more IN packets (may be a multi-packet transfer)
    // FIRST Data packet in a CTL transfer uses DATA1 toggle.
    MAX3421E.write_register(rHCTL, bmRCVTOG1);
    resultcode = in_transfer(0, setup_pkt.wLength);
    if (resultcode) {
        return (resultcode);
    }

    IN_nak_count = nak_count;
    // The OUT status stage
    resultcode = send_packet(tokOUTHS, 0);
    if (resultcode) {
        print_error(resultcode);
        // should be 0, indicating ACK. Else return error code.
        return (resultcode);
    }
    return (0);
}

static u8 in_transfer(u8 endpoint, u16 INbytes)
{
    u8 resultcode, j, pktsize;
    u32 xfrlen, xfrsize;

    xfrsize = INbytes;
    xfrlen = 0;

    while (1) {
        // IN packet to EP-'endpoint'. Function takes care of NAKS.
        resultcode = send_packet(tokIN, endpoint);
        if (resultcode) {
            // should be 0, indicating ACK. Else return error code.
            return (resultcode);
        }
        // number of received bytes
        pktsize = MAX3421E.read_register(rRCVBC);

        // add this packet's data to XfrData array
        for (j = 0; j < pktsize; j++) {
            usb_buffer[j + xfrlen] = MAX3421E.read_register(rRCVFIFO);
        }

        // Clear the IRQ & free the buffer
        MAX3421E.write_register(rHIRQ, bmRCVDAVIRQ);

        // add this packet's byte count to total transfer length
        xfrlen += pktsize;

        // The transfer is complete when something smaller than max_packet_size is sent or 'INbytes' have been transfered
        if ((pktsize < my_usb_device->device_descriptor.bMaxPacketSize0) || (xfrlen >= xfrsize)) {
            last_transfer_size = xfrlen;
            return (resultcode);
        }
    }
}
static u8 control_write_no_data(u8 bmRequestType, u8 bRequest, u8 wValueLo, u8 wValueHi, u16 wIndex, u16 wLength)
{
    SETUP_PKT setup_pkt;
    setup_pkt.ReqType_u.bmRequestType = bmRequestType;
    setup_pkt.bRequest = bRequest;
    setup_pkt.wVal_u.wValueLo = wValueLo;
    setup_pkt.wVal_u.wValueHi = wValueHi;
    setup_pkt.wIndex = wIndex;
    setup_pkt.wLength = wLength;

    u8 resultcode;
    MAX3421E.write_bytes(rSUDFIFO, 8, (u8*)&setup_pkt);
    //Send the SETUP token and 8 setup bytes. Device should immediately ACK.
    resultcode = send_packet(tokSETUP, 0);
    if (resultcode) {
        // should be 0, indicating ACK.
        return (resultcode);
    }

    //No data stage, so the last operation is to send an IN token to the peripheral
    //as the STATUS (handshake) stage of this control transfer. We should get NAK or the
    //DATA1 PID. When we get the DATA1 PID the 3421 automatically sends the closing ACK.
    resultcode = send_packet(tokINHS, 0);
    if (resultcode) {
        // should be 0, indicating ACK.
        return (resultcode);
    } else {
        return (0);
    }
}

static void wait_frames(u8 num)
{
    u8 k = 0;
    // clear any pending
    MAX3421E.write_register(rHIRQ, bmFRAMEIRQ);

    while (k != num) {
        while (!(MAX3421E.read_register(rHIRQ) & bmFRAMEIRQ))
            ;

        // clear the IRQ
        MAX3421E.write_register(rHIRQ, bmFRAMEIRQ);
        k++;
    }
}

static u8 send_packet(u8 token, u8 endpoint)
{
    u8 resultcode, retry_count = 0;
    nak_count = 0;

    // If the response is NAK or timeout, keep sending until either NAK_LIMIT or RETRY_LIMIT is reached.
    while (1) {
        // launch the transfer
        MAX3421E.write_register(rHXFR, (token | endpoint));
        // wait for the completion IRQ
        while (!(MAX3421E.read_register(rHIRQ) & bmHXFRDNIRQ))
            ;

        // clear the IRQ
        MAX3421E.write_register(rHIRQ, bmHXFRDNIRQ);

        // get the result
        resultcode = (MAX3421E.read_register(rHRSL) & 0x0F);
        if (resultcode == hrNAK) {
            nak_count++;
            if (nak_count == NAK_LIMIT) {
                break;
            } else {
                continue;
            }
        }

        if (resultcode == hrTIMEOUT) {
            retry_count++;
            if (retry_count == RETRY_LIMIT) {
                // hit the max allowed retries. Exit and return result code
                break;
            } else {
                continue;
            }
        } else {
            // all other cases, just return the success or error code
            break;
        }
    }
    return (resultcode);
}

static u8 print_error(u8 err)
{
    if (err) {
        switch (err) {
        case hrBUSY:
            log_error("MAX3421E SIE is busy ");
            break;
        case hrBADREQ:
            log_error("Bad value in HXFR register ");
            break;
        case hrNAK:
            log_error("Exceeded NAK limit");
            break;
        case hrKERR:
            log_error("LS Timeout ");
            break;
        case hrJERR:
            log_error("FS Timeout ");
            break;
        case hrTIMEOUT:
            log_error("Device did not respond in time ");
            break;
        case hrBABBLE:
            log_error("Device babbled (sent too long) ");
            break;
        default:
            log_error("Programming error %01X,", err);
        }
    }
    return (err);
}

const struct usbcore USBCore = {
    .init = init,
    .poll = poll,
    .in_transfer = in_transfer,
    .get_usb_buffer = get_usb_buffer,
    .send_packet = send_packet,
    .control_write_no_data = control_write_no_data,
    .control_read_transfer = control_read_transfer,
    .get_last_transfer_size = get_last_transfer_size,
};
