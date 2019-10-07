#include "MAX3421E.h"

static PlatformConfig platform_config;

static void write_register(u8 reg, u8 val)
{
    uint8_t out[2];
    out[0] = reg | 0x02;
    out[1] = val;
    platform_config.spi_transmit(out, 2);
}

static u8 read_register(u8 reg)
{
    uint8_t out[2], in[2];
    out[0] = reg;
    out[1] = 0;
    platform_config.spi_transmit_receive(out, 2, in, 2);
    return in[1];
}
static void read_bytes(u8 reg, u8 bytes, u8* in)
{
    uint8_t out[2];
    out[0] = reg;
    out[1] = 0;
    platform_config.spi_transmit_receive(out, bytes, in, bytes);
}

static void write_bytes(u8 reg, u8 N, u8* p)
{
    uint8_t out[9];
    out[0] = reg + 0x02; // command byte into the FIFO. 0x0002 is the write bit
    for (int j = 0; j < N; j++) {
        out[j + 1] = *p++; // send the next data byte
    }
    platform_config.spi_transmit(out, N + 1);
}

static void soft_reset(void)
{
    // chip reset This stops the oscillator
    write_register(rUSBCTL, bmCHIPRES);

    // remove the reset
    write_register(rUSBCTL, 0x00);

    // hang until the PLL stabilizes
    while (!(read_register(rUSBIRQ) & bmOSCOKIRQ))
        ;
}
static void hard_reset(void)
{
    platform_config.hard_reset();
}
static void clear_conn_detect_irq(void)
{
    write_register(rHIRQ, bmCONDETIRQ); //clear connection detect interrupt
}
static void enable_irq(void)
{
    write_register(rCPUCTL, 0x01); //enable interrupt pin
}
static void reset_bus(void)
{
    // Let the bus settle
    platform_config.delay(200);

    //issue bus reset
    write_register(rHCTL, bmBUSRST);

    // Wait for a response
    while ((read_register(rHCTL) & bmBUSRST) != 0)
        ;
    u8 tmpdata = read_register(rMODE) | bmSOFKAENAB; //start SOF generation
    write_register(rMODE, tmpdata);

    // when first SOF received _and_ 20ms has passed we can continue
    while ((read_register(rHIRQ) & bmFRAMEIRQ) == 0)
        ;
    platform_config.delay(3);
}

static void init(PlatformConfig* platform_config_in)
{
    memcpy(&platform_config, platform_config_in, sizeof(PlatformConfig));

    hard_reset();

    write_register(rPINCTL, bmFDUPSPI | bmPOSINT); // Full duplex SPI, edge-active, rising edges
    soft_reset();
    write_register(rMODE, bmDPPULLDN | bmDMPULLDN | bmHOST); // set pull-downs, Host
    write_register(rHIEN, bmCONDETIE | bmFRAMEIE); //connection detection
    write_register(rHCTL, bmSAMPLEBUS); // sample USB bus
    while (!(read_register(rHCTL) & bmSAMPLEBUS))
        ; //wait for sample operation to finish
    VBUS_OFF; // and Vbus OFF (in case something already plugged in)
    platform_config.delay(200);
    VBUS_ON;
}

const struct max3421e MAX3421E = {
    .init = init,
    .write_register = write_register,
    .read_register = read_register,
    .write_bytes = write_bytes,
    .read_bytes = read_bytes,
    .soft_reset = soft_reset,
    .hard_reset = hard_reset,
    .clear_conn_detect_irq = clear_conn_detect_irq,
    .enable_irq = enable_irq,
    .reset_bus = reset_bus,
};
