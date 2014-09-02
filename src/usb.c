/** Implementation of the USB driver */

#include <p18f2550.h>
#include <stdio.h>

#include "usb.h"
#include "usb_bd.h"
#include "usb_ctl.h"

#define USB_CFG_NUM_ENDPOINTS 16

/* 8 bytes min, 64 bytes max. Two buffers of this size are allocated -
   one for IN and one for OUT directions. */
#define USB_CFG_EP0_BUFFER_SIZE 8

typedef enum {
    USB_ST_UNATTACHED,
    USB_ST_ATTACHED,
    USB_ST_DEFAULT,
    USB_ST_ADDRESSED,
    USB_ST_CONFIGURED
} usbState;

typedef struct {
    usbState state;
    usbEvent eventBuffer;
} usbInternalState;

static usbInternalState usbState;

/* Handlers for various USB events */
usbEventHandler eventHandlers[USB_EV_MAX];

/* Application callbacks */
usbCallback userCallbacks[USB_CB_MAX];

usbError usbDetachHandler(void);
usbError usbAttachHandler(void);
usbError usbResetHandler(void);
usbError usbTransactionHandler(void);
usbError usbNop(void);

void usbInitHardware(void);
usbError usbCheckInterrupt(void);

/* This handler does nothing */
usbError usbNop()
{
    printf("usb: NOP\r\n");
    return USB_SUCCESS;
}

void usbInitHardware()
{
    /* Initialize the USB hardware */
    UCON = 0;

    /* internal pullup enabled; high speed operation;
       use on-chip transceiver; disable ping-pong */
    UCFG = 0x14;

    /* No interrupts */
    UIE = 0; UEIE = 0;
}

usbError usbInit()
{
    int eventIndex, cbIndex;
    usbError ret;

    printf("usb: Init\r\n");

    usbInitHardware();

    /* Initialize the event buffer */
    usbState.eventBuffer = USB_EV_NONE;

    /* Initialize event handlers */
    for (eventIndex = 0; eventIndex < USB_EV_MAX; eventIndex++) {
        eventHandlers[eventIndex] = usbNop;
    }
    for (cbIndex = 0; cbIndex < USB_CB_MAX; cbIndex++) {
        userCallbacks[cbIndex] = 0;
    }

    /* Initialize buffer descriptors, allocate EP0 buffers */
    usbBdInit();
    ret = usbBdSetup(0, USB_ED_OUT, USB_CFG_EP0_BUFFER_SIZE);
    if (USB_SUCCESS != ret) {
        printf("usb: BD Setup failed! ret=%d\r\n", ret);
        return ret;
    }
    ret = usbBdSetup(0, USB_ED_IN, USB_CFG_EP0_BUFFER_SIZE);
    if (USB_SUCCESS != ret) {
        printf("usb: BD Setup failed! ret=%d\r\n", ret);
        return ret;
    }

    /* Move to detached state */
    usbDetachHandler();

    return ret;
}

usbError usbDetachHandler()
{
    printf("usb: State = UNATTACHED\r\n");

    /* Disable the USB hardware */
    UCONbits.SUSPND = 0;
    UCONbits.USBEN = 0;

    usbState.state = USB_ST_UNATTACHED;
    eventHandlers[USB_EV_ATTACHED] = usbAttachHandler;
    eventHandlers[USB_EV_DETACHED] = usbNop;
    eventHandlers[USB_EV_RESET] = usbNop;
    eventHandlers[USB_EV_TRANSACTION] = usbNop;

    return USB_SUCCESS;
}

usbError usbAttachHandler() 
{
    printf("usb: State = ATTACHED\r\n");

    /* Clear interrupt status */
    UIR = 0; UEIR = 0;

    /* Enable the USB hardware */
    UCONbits.USBEN = 1;

    /* Wait for the single-ended zero condition to clear - otherwise
       we could mistake it for a Reset on the bus */
    while ((unsigned char)0 != UCONbits.SE0) {
        ;
    }
    UIRbits.URSTIF = 0;

    usbState.state = USB_ST_ATTACHED;
    eventHandlers[USB_EV_ATTACHED] = usbNop;
    eventHandlers[USB_EV_DETACHED] = usbDetachHandler;
    eventHandlers[USB_EV_RESET] = usbResetHandler;

    return USB_SUCCESS;
}

usbError usbResetHandler()
{
    char ep; char *UEPnPtr = &UEP1;

    printf("usb: Reset handler\r\n");

    /* Disable all endpoints except EP0 */
    for (ep = 1; ep < USB_CFG_NUM_ENDPOINTS; ep++) {
        *UEPnPtr = 0;
        UEPnPtr++;
    }
    /* Handshake enabled; IN+OUT; enable Control */
    UEP0 = 0x16;

    // TODO clear the transaction buffer

    /* Hand off EP0 */
    usbCtlInit();

    /* Enable USB packet processing */
    UCONbits.PKTDIS = 0;

    printf("usb: State = DEFAULT\r\n");
    usbState.state = USB_ST_DEFAULT;
    eventHandlers[USB_EV_TRANSACTION] = usbTransactionHandler;

    return USB_SUCCESS;
}

usbError usbTransactionHandler()
{
    usbBdHandle bdHandle;
    usbCallback cbNonEP0 = userCallbacks[USB_CB_TRANSACTION];

    bdHandle = usbBdGetHandleForTransaction();

    if (0 == usbBdGetEndpoint(bdHandle)) {
        /* Transactions on EP0 are handled by the USB library */
        usbCtlHandleTransaction(bdHandle);
    } else {
        /* Non-EP0 transactions should be handled by the user */
        if (USB_ST_CONFIGURED != usbState.state) {
            /* Ignore */
            printf("usb: Non-EP0 transaction in non-configured state!\r\n");
        } else {            
            if (0 == cbNonEP0) {
                printf("usb: Non-EP0 transaction and no callback!\r\n");
            } else {
                (void) cbNonEP0((void *)&bdHandle);
            }
        }
    }

    /* Finished with the transaction, advance the transaction FIFO */
    UIRbits.TRNIF = 0;
    return USB_SUCCESS;
}

usbError usbiSetAddress(char address)
{
    if ((USB_ST_DEFAULT == usbState.state) || 
        (USB_ST_ADDRESSED == usbState.state)) {
        if ((address > 0) && (address < 128)) {
            UADDR = address;
            usbState.state = USB_ST_ADDRESSED;
            printf("usb: State = ADDRESSED\r\n");
            return USB_SUCCESS;
        } else {
            printf("usb: invalid address\r\n");
            return USB_EBADPARM;
        }
    } else {
        printf("usb: Can't set address in state %d!\r\n", usbState.state);
        return USB_EBADSTATE;
    }
}

usbError usbPostEvent(usbEvent ev)
{
    if (USB_EV_NONE == usbState.eventBuffer) {
        usbState.eventBuffer = ev;
        return USB_SUCCESS;
    } else {
        printf("usb: Event Buffer Overflown!\r\n");
        return USB_EOVERFLOW;
    }
}

void usbGetEvent(usbEvent *ev)
{
    *ev = usbState.eventBuffer;
    usbState.eventBuffer = USB_EV_NONE;
}

usbError usbCheckInterrupt()
{
    /* If any interrupts are set... */
    if ((unsigned char)0 != UIR) {
        if ((unsigned char)1 == UIRbits.URSTIF) {
            UIRbits.URSTIF = 0;
            usbPostEvent(USB_EV_RESET);
            return USB_SUCCESS;
        }

        if ((unsigned char)1 == UIRbits.TRNIF) {
            /* Does not clear the interrupt bit 
               (or the transaction data will be invalid) */
            usbPostEvent(USB_EV_TRANSACTION);
            return USB_SUCCESS;
        }
        /* TODO actually handle all interrupts */
        printf("usb: Unhandled Interrupt!\r\n");
    }
    return USB_SUCCESS;
}

usbError usbWork()
{
    usbEvent ev;
    usbError ret;

    /* Handle USB events. */
    do {
        usbGetEvent(&ev);
        if (USB_EV_NONE != ev) {
            ret = eventHandlers[ev]();
            if (USB_SUCCESS != ret) {
                printf("usb: EventHandler Failed! ev=%d ret=%d\r\n", ev, ret);
                return ret;
            }
        }
        usbCheckInterrupt();

    } while (ev != USB_EV_NONE);

    return USB_SUCCESS;
}

void usbSetPowerState(usbPowerState powerState)
{
    /* Only the control EP handler cares */
    usbCtlSetPowerState(powerState);
}

usbError usbSetCallback(usbCallbackEvent cbEvent, usbCallback callback)
{
    if ((cbEvent < 0) || (cbEvent > USB_CB_MAX)) {
        printf("usb: invalid callback event %d\r\n", cbEvent);
        return USB_EBADPARM;
    }
    userCallbacks[cbEvent] = callback;
    return USB_SUCCESS;
}

usbError usbiSetConfig(unsigned char config)
{
    usbCallback cbConfig = userCallbacks[USB_CB_CONFIG];
    usbError cbRet;

    if ((USB_ST_ADDRESSED != usbState.state) && 
        (USB_ST_CONFIGURED != usbState.state)) {
        printf("usb: Can't set config in state %d!\r\n", usbState.state);
        return USB_EBADSTATE;
    }

    if (0 == cbConfig) {
        printf("usb: No callback for USB_CB_CONFIG\r\n");
        return USB_ENOIMP;
    }

    cbRet = cbConfig((void *)&config);

    if ((unsigned char)0 == config) {
        /* Go back to addressed state */
        printf("usb: State = ADDRESSED\r\n");
        usbState.state = USB_ST_ADDRESSED;
        return USB_SUCCESS;
    } else {
        if (USB_SUCCESS == cbRet) {
            printf("usb: State = CONFIGURED\r\n");
            usbState.state = USB_ST_CONFIGURED;
            return USB_SUCCESS;
        } else {
            printf("usb: user callback did not succeed for config %d\r\n",
                   config);
            /* Assuming this configuration is not supported - no state change */
            return cbRet;
        }
    }
}
