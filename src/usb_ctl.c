/* USB Control Transactions Handling implementation. */

/* This implementation supports only Endpoint 0. Most devices do not need more
   than one control endpoint. */

#include <p18f2550.h>
#include <stdio.h>

#include "usb_ctl.h"
#include "usb.h"
#include "usb_bd.h"

#include "string.h"

#define MIN(a,b) ((a)<(b))?(a):(b)

/** Possible control transaction state */
typedef enum {
    USB_CTL_SETUP, /**< Setup stage */
    USB_CTL_DATA, /**< Data stage */
    USB_CTL_STATUS /**< Status stage */
} usbCtlState;

/** Direction of the control transaction */
typedef enum {
    USB_CTL_DIR_OUT = 0,
    USB_CTL_DIR_IN = 1
} usbCtlDir;

typedef enum {
    USB_CTL_FROM_ROM,
    USB_CTL_FROM_RAM
} usbCtlSource;

/** Values for usbCtlRequestType.requestType */
typedef enum {
    USB_CTL_REQ_STANDARD = 0,
    USB_CTL_REQ_CLASS = 1,
    USB_CTL_REQ_VENDOR = 2
} usbCtlRequestTypes;

/** Values for usbCtlRequestType.recipient */
typedef enum {
    USB_CTL_REC_DEVICE = 0,
    USB_CTL_REC_INTERFACE = 1,
    USB_CTL_REC_ENDPOINT = 2,
    USB_CTL_REC_OTHER = 3
} usbCtlRecipient;

/** Standard control request types */
typedef enum {
    USB_CTL_STD_GET_STATUS = 0,
    USB_CTL_STD_CLEAR_FEATURE = 1,
    USB_CTL_STD_SET_FEATURE = 3,
    USB_CTL_STD_SET_ADDRESS = 5,
    USB_CTL_STD_GET_DESCRIPTOR = 6,
    USB_CTL_STD_SET_DESCRIPTOR = 7,
    USB_CTL_STD_GET_CONFIGURATION = 8,
    USB_CTL_STD_SET_CONFIGURATION = 9,
    USB_CTL_STD_GET_INTERFACE = 10,
    USB_CTL_STD_SET_INTERFACE = 11,
    USB_CTL_STD_SYNCH_FRAME = 12
} usbCtlStandardRequestType;

/* Non-NULL dataPtr and a zero bytesToTransfer - zero-length packet needs to
   be sent.
 
   Out of all standard ctl requests, only one requires a buffer not from ROM -
   Get_Status. Since it's small, just keep it in the state (getStatusBuf).
*/ 
typedef struct {
    usbCtlState state;
    usbCtlDir dir;
    usbBdHandle outHandle;
    usbBdHandle inHandle;
    usbCtlSource dataSource;
    char *dataPtr;
    int bytesToTransfer;
    usbPowerState powerState;
    char getStatusBuf[2];
    /** Stores the new device address until the Status stage completes */
    char newAddress;
} usbCtlInternalState;

/** Request Type bitfield in a Setup packet */
typedef struct {
    unsigned recipient:5;
    unsigned requestType:2;
    unsigned dir:1;
} usbCtlRequestType;

/** Setup packet structure */
typedef struct {
    usbCtlRequestType type;
    unsigned char request;
    unsigned int data;
    unsigned int index;
    unsigned int length;
} usbCtlSetupPacket;

static usbCtlInternalState ctlState;

void usbCtlInit(void)
{
    printf("ctl: Init\r\n");

    ctlState.state = USB_CTL_SETUP;
    ctlState.dataPtr = 0;
    ctlState.bytesToTransfer = 0;

    ctlState.getStatusBuf[0] = 0;
    ctlState.getStatusBuf[1] = 0;
    ctlState.newAddress = 0;

    (void) usbBdGetHandleForEndpoint(0, USB_ED_OUT, &ctlState.outHandle);
    (void) usbBdGetHandleForEndpoint(0, USB_ED_IN, &ctlState.inHandle);

    /* ctlState.powerState may be set before this function is called - do not
       change or reset it here! */

    /* After init, only a Setup packet should be accepted */
    usbBdStall(ctlState.outHandle);
    usbBdStall(ctlState.inHandle);
}

void usbCtlAbortTransaction(void)
{
    if (USB_CTL_SETUP != ctlState.state) {
        printf("ctl: Abort\r\n");
    }

    ctlState.state = USB_CTL_SETUP;
    ctlState.dataPtr = 0;
    ctlState.bytesToTransfer = 0;
    usbBdStall(ctlState.inHandle);
}

usbError usbCtlGetDescriptor(usbCtlSetupPacket *bufPtr)
{
    char i;
    usbError ret;
    char descType = bufPtr->data >> 8;
    char descIndex = bufPtr->data & 0xFF;

    /* Find the descriptor in the descriptor table */
    for (i=0; i<usbCtlDescriptorCount; i++) {
        if ((usbCtlDescriptorList[i].type == descType) &&
            (usbCtlDescriptorList[i].index == descIndex)) {
           printf("ctl: GetDescriptor, type=%d, index=%d\r\n",
                  descType, descIndex);

           ctlState.dataSource = USB_CTL_FROM_ROM;
           ctlState.dataPtr = usbCtlDescriptorList[i].data;
           ctlState.bytesToTransfer = MIN((int)bufPtr->length,
                                          usbCtlDescriptorList[i].totalSize);
           ctlState.state = USB_CTL_DATA;
           return USB_SUCCESS;
        }
    }
    /* Not found */
    printf("ctl: Descriptor not found! Type=%d, index=%d\r\n",
           descType, descIndex);
    return USB_EBADPARM;
}

usbError usbCtlGetStatus(usbCtlSetupPacket *bufPtr)
{
    switch (bufPtr->type.recipient) {
    case USB_CTL_REC_DEVICE:
        printf("ctl: GetStatus(dev)");
        ctlState.getStatusBuf[0] = ctlState.powerState;
        /* Remote wakeup is not supported */
        break;
    default:
        printf("ctl: GetStatus, recipient not supported (%d)",
               bufPtr->type.recipient);
        return USB_EBADPARM;
    }

    ctlState.dataPtr = ctlState.getStatusBuf;
    ctlState.dataSource = USB_CTL_FROM_RAM;
    ctlState.bytesToTransfer = sizeof(ctlState.getStatusBuf);
    ctlState.state = USB_CTL_DATA;
    return USB_SUCCESS;
}

usbError usbCtlSetAddress(usbCtlSetupPacket *bufPtr)
{
    if ((bufPtr->data > (unsigned)0) && (bufPtr->data < (unsigned)128)) {
        printf("ctl: SetAddress(%d)\r\n", bufPtr->data);

        ctlState.newAddress = bufPtr->data;
        ctlState.state = USB_CTL_STATUS;

        return USB_SUCCESS;
    } else {
        printf("ctl: Invalid address\r\n");
        return USB_EBADDATA;
    }
}

usbError usbCtlHandleSetup(void)
{
    usbCtlSetupPacket *bufPtr;
    int size;
    usbError ret = USB_SUCCESS;

    ret = usbBdGetBuf(ctlState.outHandle, (char **)&bufPtr, &size);
    if (USB_SUCCESS != ret) {
        printf("ctl: GetBuf Failed\r\n");
        return ret;
    }
    if (sizeof(usbCtlSetupPacket) != (unsigned int)size) {
        printf("ctl: Bad Setup Packet, size=%d\r\n", size);
        return USB_EBADDATA;
    }

    ctlState.dir = bufPtr->type.dir;
    ret = USB_ENOIMP;
    switch (bufPtr->type.requestType) {
    case USB_CTL_REQ_STANDARD:
        switch (bufPtr->request) {
        case USB_CTL_STD_GET_STATUS:
            ret = usbCtlGetStatus(bufPtr);
            break;
        case USB_CTL_STD_SET_ADDRESS:
            ret = usbCtlSetAddress(bufPtr);
            break;
        case USB_CTL_STD_GET_DESCRIPTOR:
            ret = usbCtlGetDescriptor(bufPtr);
            break;
        case USB_CTL_STD_SET_CONFIGURATION:
            ret = usbiSetConfig((unsigned char)(bufPtr->data));
            break;
        default:
            printf("ctl: Not Handled, r=%d\r\n", bufPtr->request);
        }
        break;
    default:
        printf("ctl: Not Handled, rt=%d, r=%d\r\n", bufPtr->type.requestType,
               bufPtr->request);
        break;
    }

    return ret;
}

usbError usbCtlLoadBufAndSend(char *buf, int bufSize)
{
    int sizeToSend = MIN(bufSize, ctlState.bytesToTransfer);
    if (USB_CTL_FROM_ROM == ctlState.dataSource) {
        memcpypgm2ram((void *)buf, (void *)ctlState.dataPtr, sizeToSend);
    } else {
        memcpy((void *)buf, (void *)ctlState.dataPtr, sizeToSend);
    }
    return usbBdSend(ctlState.inHandle, sizeToSend);
}

void usbCtlHandleIn(void)
{
    int sentSize, bufSize;
    char *buf;

    switch (ctlState.state) {
    case USB_CTL_DATA:
        if (USB_CTL_DIR_IN == ctlState.dir) {
            usbBdGetSent(ctlState.inHandle, &sentSize);
            usbBdGetBuf(ctlState.inHandle, &buf, &bufSize);
            if (sentSize < bufSize) {
                if (sentSize == ctlState.bytesToTransfer) {
                    /* Data stage complete */
                    printf("ctl: No more data to send\r\n");
                    ctlState.state = USB_CTL_STATUS;
                } else {
                    /* Should never happen: if there is more data to transfer
                       why didn't we send it in the previous transaction? */
                    printf("ctl: Unexpected condition\r\n");
                    ctlState.state = USB_CTL_SETUP;
                }
                usbBdStall(ctlState.inHandle);
                return;
            }

            /* Data stage continues, more data or 0-length packet to send */
            ctlState.bytesToTransfer = ctlState.bytesToTransfer - sentSize;
            ctlState.dataPtr = ctlState.dataPtr + sentSize;

            if (USB_SUCCESS != usbCtlLoadBufAndSend(buf, bufSize)) {
                printf("ctl: Send failed\r\n");
            }
            return;

        } else {
            /* Premature end of OUT control transfer */
            printf("ctl: OUT Aborted\r\n");
            ctlState.state = USB_CTL_SETUP;
        }

    case USB_CTL_STATUS:
        if (USB_CTL_DIR_OUT == ctlState.dir) {
            /* Control transfer complete. */
            printf("ctl: Control write complete\r\n");
            ctlState.state = USB_CTL_SETUP;

            /* If the device address was changed, perform the change now */
            if (0 != ctlState.newAddress) {
                (void) usbiSetAddress(ctlState.newAddress);
                ctlState.newAddress = 0;
            }
        }
        /* Otherwise, wrong direction for this state of the transfer. */
        /* Fallthrough! */

    case USB_CTL_SETUP:
        /* Wrong state of the transfer, SETUP should always start on OUT EP */
        printf("ctl: Stalling\r\n");
        usbBdStall(ctlState.inHandle);
    }
}

void usbCtlHandleOut(void)
{
    char *buf;
    int size;

    if (USB_SUCCESS != usbBdGetBuf(ctlState.outHandle, &buf, &size)) {
        printf("ctl: HandleOut GetBuf failed");
    }

    switch (ctlState.state) {
    case USB_CTL_DATA:
        if (USB_CTL_DIR_OUT == ctlState.dir) {
            printf("ctl: TODO HandleOut\r\n");
            /* TODO */
        } else {
            /* Premature end of IN control transfer */
            printf("ctl: IN Aborted\r\n");
            ctlState.state = USB_CTL_SETUP;
        }
        break;

    case USB_CTL_STATUS:
        /* Normal end of IN control transfer - zero-length OUT token */
        if ((USB_CTL_DIR_IN == ctlState.dir) && (0 == size)) {
            printf("ctl: Control read complete\r\n");
            ctlState.state = USB_CTL_SETUP;        
        } 
    }
    usbBdStall(ctlState.outHandle);
}

usbError usbCtlHandleTransaction(usbBdHandle bdHandle)
{
    int size;
    char *buf;
    char pid;

    if (0 != usbBdGetEndpoint(bdHandle)) {
        return USB_ENOIMP;
    }

    if (ctlState.inHandle == bdHandle) {
        usbCtlHandleIn();
    } else {
        usbBdGetPID(bdHandle, &pid);
        if (USB_PID_SETUP == pid) {
            /* A new control transfer is starting */
            usbCtlAbortTransaction();
            if (USB_SUCCESS != usbCtlHandleSetup()) {
                usbBdStall(ctlState.outHandle);
            } else {
                /* Initialize the endpoints, corresponding to stage */
                usbBdClaim(ctlState.inHandle);
                if (USB_CTL_DATA == ctlState.state) {
                    if (USB_CTL_DIR_IN == ctlState.dir) {
                        /* Load the IN endpoint with data */
                        usbBdGetBuf(ctlState.inHandle, &buf, &size);
                        usbBdSetSync(ctlState.inHandle, USB_DTS_ON, USB_DTS_DATA1);
                        if (USB_SUCCESS != usbCtlLoadBufAndSend(buf, size)) {
                            printf("ctl: Send failed\r\n");
                        }
                    } else {
                        printf("ctl: 328\r\n");
                        /* ? */
                    }
                    /* The OUT endpoint must be ready to accept status or
                       next SETUP token */
                    usbBdSetSync(ctlState.outHandle, USB_DTS_ON, USB_DTS_DATA1);
                    usbBdReceive(ctlState.outHandle);
                } else {
                    /* Control write with no data stage. Prepare the in endpoint
                       to acknowledge the write, stall the out endpoint to
                       accept the next SETUP token */
                    usbBdSend(ctlState.inHandle, 0);
                    usbBdStall(ctlState.outHandle);
                }
            }
            UCONbits.PKTDIS = 0; /* was set when the setup token was received */
        } else {
            usbCtlHandleOut();
        }
    } 
    return USB_SUCCESS;
}

void usbCtlSetPowerState(usbPowerState powerState)
{
    ctlState.powerState = powerState;
}
