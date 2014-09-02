/** USB Control Transactions Handling header  */

#ifndef USB_CTL_H
#define USB_CTL_H

#include "usb.h"
#include "usb_bd.h"

typedef struct {
    char type;
    char index;
    int totalSize;
    char *data;
} usbCtlDescriptor;

/** The descriptor list
   
    This is a symbol you must define in your program. Without the list
    (and the associated count) the USB library will not link. You must
    include all the mandatory descriptors; ensure descriptor correctness; and
    avoid duplication, as the first match will be returned.
*/
extern const rom usbCtlDescriptor usbCtlDescriptorList[];
extern const rom char usbCtlDescriptorCount;

/** Initialize the control transactions state */
void usbCtlInit(void);

/** Tell the ctl handler whether the device is self-powered or bus-powered */
void usbCtlSetPowerState(usbPowerState powerState);

/** Process a EP0 transaction that may be a part of control transfer */
usbError usbCtlHandleTransaction(usbBdHandle bdHandle);

#endif /* USB_CTL_H */
