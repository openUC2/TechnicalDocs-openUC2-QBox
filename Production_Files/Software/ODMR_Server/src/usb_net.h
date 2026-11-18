// USB network (CDC-NCM) next to the USB serial port, ESP32-S3 with Arduino
// core 3 in TinyUSB mode only: the host (iPad, iPhone, computer) gets
// 192.168.7.x by DHCP and reaches the web server at http://192.168.7.1.
// Other builds get empty stubs.
#pragma once

#include <Arduino.h>

#if defined(CONFIG_TINYUSB_NCM_ENABLED) && CONFIG_TINYUSB_NCM_ENABLED && !ARDUINO_USB_MODE
#define HAS_USB_NET 1

/** Starts the network on the USB port. Call once in setup(). */
void usbNetBegin();

/** True while a host has been talking over the USB network in the last few seconds. */
bool usbNetActive();

#else
#define HAS_USB_NET 0
inline void usbNetBegin() {}
inline bool usbNetActive() { return false; }
#endif

/** Clients of the USB network: 192.168.7.x. */
inline bool isUsbNetClient(const IPAddress &ip)
{
  return ip[0] == 192 && ip[1] == 168 && ip[2] == 7;
}
