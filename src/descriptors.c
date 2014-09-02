#include <p18f2550.h>
#include "usb_ctl.h"

#pragma romdata descriptor_table

const rom char usbDeviceDescriptor[] =
{
    18, // Size in bytes
    1, // Device Descriptor
    0x01, 0x01, // USB 1.1 compliant
    0, 0, 0, // Class/subclass/protocol
    8, // EP0 max size
    0xD8, 0x04, // Vendor ID
    0x01, 0x00, // Product ID
    0x01, 0x00, // Device version (BCD)
    0, // Manufacturer string
    0, // Product string
    0, // Serial number string
    1  // Number of configurations
};

const rom char usbConfigurationDescriptor[] =
{
    9, // Size in bytes
    2, // Configuration Descriptor
    34, 0, // Total size in bytes
    1, // Number of interfaces
    1, // Configuration index
    0, // Configuration string
    0x40, // Self-powered
    50, // 100 mA power consumption

    9, // Size in bytes
    4, // Interface Descriptor
    0, // Interface number
    0, // Alternate setting number
    1, // Number of endpoints, excluding EP0
    3, // HID Class
0, // Subclass
0, // Protocol
    0, // Interface string

    9, // Size in bytes
    0x21, // HID Descriptor
    0x01, 0x01, // HID 1.1 Compliant
    0, // Country code (0 = not localized)
    1, // Number of subordinate descriptors
    0x22, // Descriptor type (report)
    0x15, 0x00, // Report descriptor size in bytes

    7, // Size in bytes
    5, // Endpoint Descriptor
    0x81, // Endpoint and direction. Bit 7: OUT=0, IN=1
    3, // 0=Control, 1=Isochronous, 2=Bulk, 3=Interrupt
    0x40, 0x00, // Max packet size (0-1023)
    0x64 // Max polling latency, ms for Interrupt
};

const rom char usbHIDReportDescriptor[] = 
{
    0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x02,                    //   USAGE (Vendor Usage 2)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x95, 0x02,                    //   REPORT_COUNT (2)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
};

const rom usbCtlDescriptor usbCtlDescriptorList[] =
{
    {1, 0, sizeof(usbDeviceDescriptor), (char *)usbDeviceDescriptor},
    {2, 0, sizeof(usbConfigurationDescriptor), (char *)usbConfigurationDescriptor},
    {0x22, 0, sizeof(usbHIDReportDescriptor), (char *)usbHIDReportDescriptor}
};

const rom char usbCtlDescriptorCount = sizeof(usbCtlDescriptorList) / 
                                       sizeof(usbCtlDescriptor);

#pragma romdata
