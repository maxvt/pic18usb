pic18usb
========

A USB stack for a HID class device on a PIC18 (PIC18F2550) microcontroller.

I started working on this because the USB was interesting and all the devices
using a serial-to-USB instead of a proper interface always seemed too hacky.
The default Microchip USB stack, now a part of [Microchip Libraries for
Applications](http://www.microchip.com/pagehandler/en-us/devtools/mla/home.html), suffers from bloat of supporting too many different devices and modes
of operation (USB OTG in a 8-bit MCU?) even if the code was nice and readable.

Anyway, it has been a nice educational experience. This code enumerates
correctly and the most recent effort was on getting the HID class reports
working. Unfortunately, Raspberry Pis and Arduinos made making small, connected
peripherals much easier, and it's hard to justify putting more effort into
bringing this project to completion at this point. If you want, fork it and 
use whatever's already there - USB descriptor management, control endpoint
and enumeration processing, and beginnings of HID support.