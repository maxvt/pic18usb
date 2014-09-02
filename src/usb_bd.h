/** USB Buffer Descriptor management header
 
    The buffer descriptors should be set up before enabling a USB 
    endpoint.
*/

#ifndef USB_BD_H
#define USB_BD_H

#include "usb.h"

typedef enum {
    USB_DTS_ON = 1,
    USB_DTS_OFF = 0
} usbBdSyncMode;

typedef enum {
    USB_DTS_DATA0 = 0,
    USB_DTS_DATA1 = 1
} usbBdSyncVal;

/** An opaque Buffer Descriptor handle. */
typedef char usbBdHandle;

/** Power-up initialization (zeroing) of the BD Table
 
    The 2550 datasheet says the UOWN bit of each BD must be
    configured before enabling the USB module.
*/
void usbBdInit(void);

/** Allocate an endpoint memory buffer
 
    The setup should be performed only once. The setup must be
    performed sequentially (rising order of endpoints, out
    direction first).
*/
usbError usbBdSetup(char endpoint, usbEndpointDirection dir, unsigned int size);

/** Returns the endpoint handle used in the currently
    processed transaction */
usbBdHandle usbBdGetHandleForTransaction(void);

/** Returns the endpoint handle */
usbError usbBdGetHandleForEndpoint(char endpoint, usbEndpointDirection dir, usbBdHandle *handle);

/** Get the direction (OUT/IN) for a handle */
usbEndpointDirection usbBdGetDirection(usbBdHandle handle);

/** Get the endpoint number for a handle */
char usbBdGetEndpoint(usbBdHandle handle);

/** Get the PID received on an endpoint */
usbError usbBdGetPID(usbBdHandle handle, char *pid);

/** Get a pointer to the endpoint's data buffer.
   
    If the endpoint's direction is OUT, the size is the size of the received
    packet. If the endpoint's direction is IN, the size is the size of the
    endpoint's buffer available for writing. */
usbError usbBdGetBuf(usbBdHandle handle, char **buf, int *size);

/** One-time stall on an endpoint.
    
    This function transfers ownership of the endpoint to SIE. */
usbError usbBdStall(usbBdHandle handle);

/** Commit an IN endpoint's buffer to be sent out.
   
    This function transfers ownership of the endpoint to SIE. */
usbError usbBdSend(usbBdHandle handle, int size);

/** Get a count of bytes sent from an IN endpoint during the last transaction.
*/
usbError usbBdGetSent(usbBdHandle handle, int *size);

/** Commit an OUT endpoint's buffer to receive data.
    
    This function transfers ownership of the endpoint to SIE. */
usbError usbBdReceive(usbBdHandle handle);

/** Set DATA0/DATA1 check mode and expected value. */
usbError usbBdSetSync(usbBdHandle handle, usbBdSyncMode mode, usbBdSyncVal value); 

/** Force an endpoint under microprocessor control.
   
    Ensure SIE is not processing packets when this is called. */
usbError usbBdClaim(usbBdHandle bdHandle);

#endif /* USB_BD_H */
