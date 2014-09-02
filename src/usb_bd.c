/* USB Buffer Descriptor (BD) management implementation */

/* TODO Assumes no ping-pong on any endpoint. */

/* usbBdHandle is just an index into the usbBdt. It depends
   on endpoint, in/out direction, and whether ping-pong is
   enabled (and when it is, on whether it's an even or odd transfer ) */

#include <p18f2550.h>

#include "usb_bd.h"
#include "usb.h"

#include "string.h"

/** Max number of buffer descriptors */
#define USB_CFG_NUM_BDS 32 /* up to 64 with ping pong */

/** Start of the USB endpoint memory buffer, past the BDT */
/* This is not optimal because in many cases, most of BDT is empty */
#define USB_CFG_ENDPOINT_BUFFER_ORIGIN 0x500

/** USB endpoint memory buffer size */
#define USB_CFG_ENDPOINT_BUFFER_SIZE 0x300

/* Extract the endpoint number from the USTAT register */
#define USTAT_EP ((USTAT & 0x78) >> 3)

#define MIN(a,b) ((a)<(b))?(a):(b)

/** Buffer Descriptor Status register (BDnSTAT) */
typedef union {
    unsigned char val; /**< Entire register */

    /* CPU mode */
    struct {
        unsigned BC:2;     /**< BC8, BC9 - byte count */
        unsigned BSTALL:1; /**< Buffer Stall Enable */
        unsigned DTSEN:1;  /**< Data Toggle Sync Enable */
        unsigned INCDIS:1; /**< Address Increment Disable (SPP only) */
        unsigned KEN:1;    /**< BD Keep Enable (SPP only) */
        unsigned DTS:1;    /**< Data Toggle Sync (ignored unless DTSEN = 1) */
        unsigned UOWN:1;   /**< 0 - CPU owns this BD */
    };

    /* SIE mode */
    struct {
        unsigned BC:2;     /**< BC8, BC9 - byte count */
        unsigned PID:4;    /**< Token PID of the last transfer */
        unsigned :1;
        unsigned UOWN:1;   /**< 1 - SIE owns this BD */
    };
} usbBdStat;

/** Buffer Descriptor (BD) structure */
typedef struct {
    usbBdStat stat; /**< BDnSTAT - status */
    unsigned char cnt; /**< BDnCNT - byte count (+2 bits in stat) */
    char *addr; /**< BDnADRL, BDnADRH - buffer address */
} usbBd;

/* Define the USB Buffer Descriptor table in memory */
#pragma udata usb4=0x400
    volatile usbBd usbBdt[USB_CFG_NUM_BDS];
#pragma udata

char *endOfAllocatedBuffer;
/* This is the highest EP that has been set up. It is used to calculate size */
usbBdHandle highestSetupBD;

usbError usbBdGetHandleForEndpoint(char endpoint, usbEndpointDirection dir, char *handle)
{
    char endpointIndex;

    if (endpoint >= USB_MAX_ENDPOINTS) {
        return USB_EBADPARM;
    }

    endpointIndex = endpoint*2;
    if (USB_ED_IN == dir) {
        endpointIndex++;
    }
    *handle = endpointIndex;
    return USB_SUCCESS;
}

usbBdHandle usbBdGetHandleForTransaction()
{
    unsigned char ep = USTAT_EP;
    return ep*2 + USTATbits.DIR;
}

void usbBdInit()
{
    (void) memset((void *)usbBdt, 0, sizeof(usbBd)*USB_CFG_NUM_BDS);
    endOfAllocatedBuffer = (char *)USB_CFG_ENDPOINT_BUFFER_ORIGIN;
    highestSetupBD = 0;
}

/* The size is the difference between this BD's address and the next
   allocated BD's address. For the last allocated BD, this is the difference
   between its address and end of allocated buffer. This works because of
   sequential setup requirement. If there are few gaps in the BDT, it is
   also relatively efficient, and saves storage by not storing the size
   explicitly for each BD. */
int usbBdGetSize(usbBdHandle handle)
{
    int size = endOfAllocatedBuffer - usbBdt[handle].addr;
    usbBdHandle loopHandle = handle + 1;

    while (loopHandle <= highestSetupBD) {
        if (0 != usbBdt[loopHandle].addr) {
            size = usbBdt[loopHandle].addr - usbBdt[handle].addr;
            break;
        }
    }

    return size;
}

void usbBdResetSize(usbBdHandle handle)
{
    int size = usbBdGetSize(handle);

    usbBdt[handle].cnt = size & 0xFF;
    usbBdt[handle].stat.BC = size >> 8;
}

usbError usbBdSetup(char endpoint, usbEndpointDirection dir, unsigned int size)
{
    usbBdHandle handle;
    unsigned int allocatedBufferSize;

    if ((endpoint >= USB_MAX_ENDPOINTS) || ((unsigned char)0 == size)) {
        return USB_EBADPARM;
    }

    /* Do not allow out-of-order initialization */
    (void) usbBdGetHandleForEndpoint(endpoint, dir, &handle);
    if (handle < highestSetupBD) {
        return USB_ERROR;
    }

    /* Do not allow initialization of the same BD twice */
    if (0 != usbBdt[handle].addr) {
        return USB_ERROR;
    }

    allocatedBufferSize = endOfAllocatedBuffer - (char *)USB_CFG_ENDPOINT_BUFFER_ORIGIN;
    if (USB_CFG_ENDPOINT_BUFFER_SIZE - allocatedBufferSize < size) {
        return USB_ENOMEM;
    }

    usbBdt[handle].addr = endOfAllocatedBuffer;

    endOfAllocatedBuffer = endOfAllocatedBuffer + size;
    highestSetupBD = handle;

    usbBdResetSize(handle);
    return USB_SUCCESS;
}

usbError usbBdGetPID(usbBdHandle bdHandle, char *pid)
{
    if ((bdHandle >= USB_CFG_NUM_BDS)) {
        return USB_EBADPARM;
    }

    if (usbBdt[bdHandle].stat.UOWN == (unsigned char)1) {
        return USB_EACCESS;
    } else {
        *pid = usbBdt[bdHandle].stat.PID;
        return USB_SUCCESS;
    }
}

usbError usbBdRelease(usbBdHandle handle)
{
    usbBdt[handle].stat.UOWN = 1;
    return USB_SUCCESS;
}

usbError usbBdClaim(usbBdHandle handle)
{
    if ((handle >= USB_CFG_NUM_BDS)) {
        return USB_EBADPARM;
    }
    usbBdt[handle].stat.UOWN = 0;
    return USB_SUCCESS;
}

usbEndpointDirection usbBdGetDirection(usbBdHandle handle)
{
    if (0 == (handle & 1)) {
        return USB_ED_OUT;
    } else {
        return USB_ED_IN;
    }
}

char usbBdGetEndpoint(usbBdHandle handle)
{
    return handle >> 1;
}

usbError usbBdGetBuf(usbBdHandle handle, char **buf, int *size)
{
    int bc;

    if ((handle >= USB_CFG_NUM_BDS)) {
        return USB_EBADPARM;
    }
    
    if (usbBdt[handle].stat.UOWN == (unsigned char)1) {
        return USB_EACCESS;
    }    
    
    if (0 == usbBdt[handle].addr) {
        return USB_ERROR; /* This BD has not been initialized */
    }

    *buf = usbBdt[handle].addr;
    if (USB_ED_OUT == usbBdGetDirection(handle)) {
        bc = usbBdt[handle].stat.BC;
        *size = usbBdt[handle].cnt + (bc << 8);
    } else {
        *size = usbBdGetSize(handle);
    }
    return USB_SUCCESS;
}

usbError usbBdGetSent(usbBdHandle handle, int *size)
{
    if ((handle >= USB_CFG_NUM_BDS) || (USB_ED_IN != usbBdGetDirection(handle))) {
        return USB_EBADPARM;
    }

    if (usbBdt[handle].stat.UOWN == (unsigned char)1) {
        return USB_EACCESS;
    }

    *size = (usbBdt[handle].stat.BC << 8) + usbBdt[handle].cnt;
    return USB_SUCCESS;
}

usbError usbBdStall(usbBdHandle handle)
{
    if ((handle >= USB_CFG_NUM_BDS)) {
        return USB_EBADPARM;
    }
    
    if (usbBdt[handle].stat.UOWN == (unsigned char)1) {
        return USB_EACCESS;
    }

    usbBdt[handle].stat.BSTALL = 1;

    usbBdResetSize(handle);
    return usbBdRelease(handle);
}

usbError usbBdReceive(usbBdHandle handle)
{
    int size;

    if ((handle >= USB_CFG_NUM_BDS) || (USB_ED_OUT != usbBdGetDirection(handle))) {
        return USB_EBADPARM;
    }
    
    if (usbBdt[handle].stat.UOWN == (unsigned char)1) {
        return USB_EACCESS;
    }
    
    size = usbBdGetSize(handle);
    
    usbBdt[handle].cnt = size & 0xFF;
    usbBdt[handle].stat.BC = size >> 8;
    usbBdt[handle].stat.BSTALL = 0;
    usbBdRelease(handle);

    return USB_SUCCESS;    
}

usbError usbBdSend(usbBdHandle handle, int size)
{
    int epSize;

    if ((handle >= USB_CFG_NUM_BDS) || (USB_ED_IN != usbBdGetDirection(handle))) {
        return USB_EBADPARM;
    }
    
    if (usbBdt[handle].stat.UOWN == (unsigned char)1) {
        return USB_EACCESS;
    }
    
    epSize = usbBdGetSize(handle);
    if (epSize < size) {
        return USB_EBADPARM;
    }
    
    usbBdt[handle].cnt = size & 0xFF;
    usbBdt[handle].stat.BC = size >> 8;
    usbBdt[handle].stat.BSTALL = 0;
    usbBdRelease(handle);

    return USB_SUCCESS;
}

usbError usbBdSetSync(usbBdHandle handle, usbBdSyncMode mode, usbBdSyncVal value)
{
    if ((handle >= USB_CFG_NUM_BDS)) {
        return USB_EBADPARM;
    }
    
    if (usbBdt[handle].stat.UOWN == (unsigned char)1) {
        return USB_EACCESS;
    }

    usbBdt[handle].stat.DTSEN = mode;
    usbBdt[handle].stat.DTS = value;

    return USB_SUCCESS;
}

