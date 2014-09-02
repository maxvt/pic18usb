#include <p18f2550.h>
#include "usb.h"
#include "usb_bd.h"
#include <stdio.h>
#include <usart.h>
#include <string.h>

#include "protocol.h"

#pragma config WDT = OFF
#define DATA_ENDPOINT_SIZE 32

void CheckForUSBAttachDetach();
unsigned char senseWaitCounter = 0;
unsigned char sensePrevValue = 0;

statusType statusBuf;

void CheckForUSBAttachDetach() 
{
  unsigned char senseValue;

  /* If the USB sense pin recently changed state, wait */
  if (senseWaitCounter > (unsigned char)0) {
      senseWaitCounter--;
      return;
  }
  
  /* Whenever a USB sense pin changes state, post attach/detach events. */
  senseValue = PORTC & 0x01;
  if (senseValue != sensePrevValue) {
      sensePrevValue = senseValue;
      senseWaitCounter = 0xFF;
      if (senseValue) {
          usbPostEvent(USB_EV_ATTACHED);
      } else {
          usbPostEvent(USB_EV_DETACHED);
      }
  }
}

void SendStatusUpdate(void)
{
    char handle;
    usbError ret;
    char *buf;
    int bufSize;

    usbBdGetHandleForEndpoint(1, USB_ED_IN, &handle);
    ret = usbBdGetBuf(handle, &buf, &bufSize);
    if (USB_EACCESS == ret) {
        printf("No access to EP1 - previous buffer?\r\n");
    } else {
        statusBuf.dummy1 = 1;
        statusBuf.dummy2 = 2;
        memcpy(buf, (void *)(&statusBuf), sizeof(statusBuf));
        ret = usbBdSend(handle, sizeof(statusBuf));
        if (USB_SUCCESS != ret) {
            printf("Send failed %d\r\n", ret);
        }
    }
}

usbError SetConfigCallback(void *param)
{
    unsigned char *config = (unsigned char *)param;
    if ((unsigned)1 == *config) {
        SendStatusUpdate();
        return USB_SUCCESS;
    } else {
        return USB_EBADPARM;
    }
}

void main (void)
{
  usbError ret;

  /* Configure the USB Sense pin - C0 */
  TRISC = TRISC & 1;
  PORTC = 0;

  OpenUSART( USART_TX_INT_OFF  & USART_RX_INT_OFF &
             USART_ASYNCH_MODE & USART_EIGHT_BIT  &
             USART_CONT_RX,
             51);

  printf("USB Project Debug Output\r\n");

  (void)usbInit();
  usbSetPowerState(USB_POWER_SELF);
  usbSetCallback(USB_CB_CONFIG, SetConfigCallback);
  ret = usbBdSetup(1, USB_ED_OUT, DATA_ENDPOINT_SIZE);
  if (USB_SUCCESS != ret) {
    printf("Data BD Setup failed! ret=%d\r\n", ret);
    return;
  }
  ret = usbBdSetup(1, USB_ED_IN, DATA_ENDPOINT_SIZE);
  if (USB_SUCCESS != ret) {
    printf("Data BD Setup failed! ret=%d\r\n", ret);
    return;
  }

  while (1) {
    CheckForUSBAttachDetach();
    (void)usbWork();
  }
}