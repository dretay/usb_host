#include "UsbDescriptorParser.h"

static PlatformConfig platform_config;

static void get_descriptor_string(u8 index, char* output, u32 size)
{
    u32* buffer_size = USBCore.get_last_transfer_size();
    u8* usb_buffer = USBCore.get_usb_buffer();
    if (index != 0) {
        if (!USBCore.control_read_transfer(bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, index, USB_DESCRIPTOR_STRING, 0, 0x40)) {
            int output_idx = 0;
            for (int i = 2; i < *buffer_size; i += 2) {
                output[output_idx] = usb_buffer[i];
                output_idx++;
                if (output_idx >= size) {
                    break;
                }
            }
        }
    }
}

static void peek_device_descriptor(USBDevice* my_usb_device)
{
    if (!USBCore.control_read_transfer(bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 0, USB_DESCRIPTOR_DEVICE, 0, 8)) {
        u32* buffer_size = USBCore.get_last_transfer_size();
        u8* usb_buffer = USBCore.get_usb_buffer();
        USB_DEVICE_DESCRIPTOR* descriptor = (USB_DEVICE_DESCRIPTOR*)usb_buffer;
        memcpy(&my_usb_device->device_descriptor, usb_buffer, sizeof(USB_DEVICE_DESCRIPTOR));

        log_debug("EP0 maxPacketSize is %02u bytes", descriptor->bMaxPacketSize0);
    }
}
static void parse_device_descriptor(USBDevice* my_usb_device)
{
    u32* buffer_size = USBCore.get_last_transfer_size();
    u8* usb_buffer = USBCore.get_usb_buffer();
    USB_DEVICE_DESCRIPTOR* descriptor = (USB_DEVICE_DESCRIPTOR*)usb_buffer;
    log_info("Device Descriptor ", 0);

    if (!USBCore.control_read_transfer(bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 0, USB_DESCRIPTOR_DEVICE, 0, descriptor->bLength)) {

        memcpy(&my_usb_device->device_descriptor, usb_buffer, sizeof(USB_DEVICE_DESCRIPTOR));

        log_trace("\tThis device has %u configuration", descriptor->bNumConfigurations);
        log_info("\tVendor  ID: 0x%04X", descriptor->idVendor);
        log_info("\tProduct ID: 0x%04X", descriptor->idProduct);

        char product_name[255] = { 0 };
        get_descriptor_string(my_usb_device->device_descriptor.iProduct, product_name, 255);
        log_info("\tProduct: %s", product_name);

        char manufacturer_name[255] = { 0 };
        get_descriptor_string(my_usb_device->device_descriptor.iManufacturer, manufacturer_name, 255);
        log_info("\tManufacturer: %s", manufacturer_name);
    }
}

static void parse_config_descriptor(USBDevice* my_usb_device)
{

    // Get the 9-byte configuration descriptor
    u32* buffer_size = USBCore.get_last_transfer_size();
    u8* usb_buffer = USBCore.get_usb_buffer();

    if (!USBCore.control_read_transfer(bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 0, USB_DESCRIPTOR_CONFIGURATION, 0, 9)) {
        USB_CONFIGURATION_DESCRIPTOR* prelim_config = (USB_CONFIGURATION_DESCRIPTOR*)usb_buffer;
        int descriptor_offset = 0;
        if (!USBCore.control_read_transfer(bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 0, USB_DESCRIPTOR_CONFIGURATION, 0, prelim_config->wTotalLength)) {
            do {
                uint8_t wDescriptorLength = usb_buffer[descriptor_offset];
                uint8_t bDescrType = usb_buffer[descriptor_offset + 1];

                switch (bDescrType) {
                case USB_DESCRIPTOR_CONFIGURATION: {
                    USB_CONFIGURATION_DESCRIPTOR* descriptor = (USB_CONFIGURATION_DESCRIPTOR*)&usb_buffer[descriptor_offset];
                    memcpy(&my_usb_device->configuration_descriptor, &usb_buffer[descriptor_offset], sizeof(USB_CONFIGURATION_DESCRIPTOR));
                    log_trace("Configuration Descriptor");
                    log_trace("\tbLength: %d", descriptor->bLength);
                    log_trace("\tbDescriptorType: %d", descriptor->bDescriptorType);
                    log_trace("\twTotalLength: %d", descriptor->wTotalLength);
                    log_trace("\tbNumInterfaces: %d", descriptor->bNumInterfaces);
                    log_trace("\tbConfigurationValue: %d", descriptor->bConfigurationValue);
                    log_trace("\tiConfiguration: %d", descriptor->iConfiguration);

                    log_trace("\tThis device is ", 0);
                    if (descriptor->bmAttributes & 0x40) {
                        log_append("self-powered");
                    } else {
                        log_append("bus powered and uses %03u milliamps", descriptor->bMaxPower * 2);
                    }
                }
                case USB_DESCRIPTOR_INTERFACE: {
                    USB_INTERFACE_DESCRIPTOR* descriptor = (USB_INTERFACE_DESCRIPTOR*)&usb_buffer[descriptor_offset];
                    memcpy(&my_usb_device->interface_descriptor, &usb_buffer[descriptor_offset], sizeof(USB_INTERFACE_DESCRIPTOR));
                    log_trace("Interface Descriptor");
                    log_trace("\tbLength: %d", descriptor->bLength);
                    log_trace("\tbDescriptorType: %d", descriptor->bDescriptorType);
                    log_trace("\tbInterfaceNumber: %d", descriptor->bInterfaceNumber);
                    log_trace("\tbAlternateSetting: %d", descriptor->bAlternateSetting);
                    log_trace("\tbNumEndpoints: %d", descriptor->bNumEndpoints);
                    log_trace("\tbInterfaceClass: %d", descriptor->bInterfaceClass);
                    log_trace("\tbInterfaceSubClass: %d", descriptor->bInterfaceSubClass);
                    log_trace("\tbInterfaceProtocol: %d", descriptor->bInterfaceProtocol);
                    log_trace("\tiInterface: %d", descriptor->iInterface);
                    break;
                }
                case HID_DESCRIPTOR_HID: {
                    USB_HID_DESCRIPTOR* descriptor = (USB_HID_DESCRIPTOR*)&usb_buffer[descriptor_offset];
                    memcpy(&my_usb_device->hid_descriptor, &usb_buffer[descriptor_offset], sizeof(USB_HID_DESCRIPTOR));
                    log_trace("HID Descriptor");
                    log_trace("\tbLength: %d", descriptor->bLength);
                    log_trace("\tbDescriptorType: %d", descriptor->bDescriptorType);
                    log_trace("\tbcdHID: %d", descriptor->bcdHID);
                    log_trace("\tbCountryCode: %d", descriptor->bCountryCode);
                    log_trace("\tbNumDescriptors: %d", descriptor->bNumDescriptors);
                    log_trace("\tbDescriptorType: %d", descriptor->bDescriptorType);
                    log_trace("\twDescriptorLength: %d", descriptor->wDescriptorLength);
                    break;
                }
                case USB_DESCRIPTOR_ENDPOINT: {
                    // check for endpoint descriptor type
                    USB_ENDPOINT_DESCRIPTOR* descriptor = (USB_ENDPOINT_DESCRIPTOR*)&usb_buffer[descriptor_offset];
                    memcpy(&my_usb_device->endpoint_descriptor, &usb_buffer[descriptor_offset], sizeof(USB_ENDPOINT_DESCRIPTOR));
                    my_usb_device->endpoint_descriptor.bEndpointAddress = (descriptor->bEndpointAddress & 0x0F);
                    log_trace("Endpoint %u", (descriptor->bEndpointAddress & 0x0F));
                    if (descriptor->bEndpointAddress & 0x80) {
                        log_append("-IN ");
                    } else {
                        log_append("-OUT ");
                    }
                    log_append("(%02u) is type ", (u8)descriptor->wMaxPacketSize);

                    switch (descriptor->bmAttributes & 0x03) {
                    case USB_TRANSFER_TYPE_CONTROL:
                        log_append("CONTROL");
                        break;
                    case USB_TRANSFER_TYPE_ISOCHRONOUS:
                        log_append("ISOCHRONOUS");
                        break;
                    case USB_TRANSFER_TYPE_BULK:
                        log_append("BULK");
                        break;
                    case USB_TRANSFER_TYPE_INTERRUPT:
                        log_append("INTERRUPT with a polling interval of %u msec.", descriptor->bInterval);
                    }
                    break;
                }
                }
                descriptor_offset += wDescriptorLength;
            } while (descriptor_offset < *buffer_size);
        }
    }
}
static void init(PlatformConfig* platform_config_in)
{
    memcpy(&platform_config, platform_config_in, sizeof(PlatformConfig));
}
const struct usbdescriptorparser UsbDescriptorParser = {
    .init = init,
    .peek_device_descriptor = peek_device_descriptor,
    .parse_device_descriptor = parse_device_descriptor,
    .parse_config_descriptor = parse_config_descriptor,
    .get_descriptor_string = get_descriptor_string,
};
