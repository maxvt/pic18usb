/** USB Driver Definitions */

#ifndef USB_H
#define USB_H

/* Defined by USB spec */
#define USB_MAX_ENDPOINTS 16

typedef enum {
    USB_SUCCESS, /**< Operation successful */
    USB_EBADPARM, /**< Parameter is invalid or out of bounds */
    USB_EBADDATA, /**< Invalid/out-of-bounds value received from the USB host */
    USB_ENOMEM, /**< Out of memory */
    USB_EOVERFLOW, /**< Internal event buffers have ran out; call usbWork() more often */
    USB_EACCESS, /**< Attempted to access memory/data not owned by CPU (owned by SIE) */
    USB_ENOIMP, /**< Not implemented or not supported */
    USB_EBADSTATE, /**< The operation is not supported in the current stack state */
    USB_ERROR /**< Unspecified error */
} usbError;

typedef enum {
    USB_EV_NONE, /**< No event (empty event) */
    USB_EV_ATTACHED, /**< USB plugged into the host. Posted from application. */
    USB_EV_DETACHED, /**< USB disconnected from the host. Posted from application. */
    USB_EV_RESET, /**< Reset command received from the host. Posted from interrupt. */
    USB_EV_TRANSACTION, /**< USB transaction has completed. Posted from interrupt. */
    USB_EV_MAX
} usbEvent;

typedef enum {
    /** Set configuration command received from the host. The callback must
        verify the configuration index passed to it (unsigned char *) and
        perform corresponding changes. If the index is valid, USB_SUCCESS
        should be returned by the callback. */
    USB_CB_CONFIG,

    /** Handle a user transaction (non-endpoint 0). The callback is called
        when an IN or OUT transaction completes on a non-EP0 endpoint. The
        callback can rearm the endpoint for another send or receive
        operation, as well as react to a received command. The callback
        receives an endpoint handle (usbBdHandle *) on which the transaction
        has occurred. */
    USB_CB_TRANSACTION,
    USB_CB_MAX
} usbCallbackEvent;

/** USB packet identifier values */
typedef enum {
    USB_PID_SETUP = 13
} usbPID;

typedef enum {
    USB_ED_OUT, /**< Endpoint direction from host to device */
    USB_ED_IN /**< Endpoint direction from device to host */
} usbEndpointDirection;

typedef enum {
    USB_POWER_BUS = 0,
    USB_POWER_SELF = 1
} usbPowerState;

typedef usbError (*usbEventHandler)(void);

typedef usbError (*usbCallback)(void *);

/** Initialize the USB driver */
usbError usbInit(void);

/** Pass an event to the USB driver */
usbError usbPostEvent(usbEvent ev);

/** Tell the USB stack whether the device is self-powered or bus-powered */
void usbSetPowerState(usbPowerState powerState);

/** Register an application callback for the specified event.
    
    The previous callback will not be called anymore, meaning only one
    callback may be registered for the particular event. */
usbError usbSetCallback(usbCallbackEvent cbEvent, usbCallback callback);

/** Call this function often to perform USB tasks */
usbError usbWork(void);

/* The following functions are internal to the USB library and should not
   be called by the application directly.*/

/** Change the device's bus address
   
    This is usually done by the control transfer handler once the Set_Status
    USB control request arrives. The application should not use this function
    directly. */
usbError usbiSetAddress(char address);

/** Change the device's configuration
    
    This is done by the control transfer handler. The function will call the
    user callback for USB_CB_CONFIG and, if successful, the configuration
    will be changed. */
usbError usbiSetConfig(unsigned char config);

#endif /* USB_H */
