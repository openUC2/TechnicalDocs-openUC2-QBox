# ESP32-S3 with USB serial and USB network together

A test for reaching the board from an iPhone or iPad without WiFi, Bluetooth or
an app, while computers and Android keep their USB serial port. The XIAO ESP32-S3
appears as two devices on one cable: a serial port (CDC-ACM) and a network
adapter (CDC-NCM).

```
iPhone/iPad ── USB-C data cable ── XIAO ESP32-S3
  Settings shows "Ethernet"          USB network adapter, 192.168.7.1
  receives 192.168.7.x by DHCP       no router offered: the phone keeps its internet
  Safari: http://192.168.7.1   ───►  web server: / and /status

Computer, Android ── same cable ──►  USB serial: PING, VERSION
                                     firmware uploads, without a button
```

This is not the ODMR firmware, and flashing it replaces the ODMR firmware on that
board. It uses ESP-IDF because Arduino has no USB network class. WiFi and
Bluetooth are not used.

## Versions

| Version | USB | Upload |
|---|---|---|
| `serial_network` (default) | Serial + network | `pio run -t upload`, any time |
| `rescue` | Same firmware | In the 10 s after plugging in |
| `network_only` | Network | In the 10 s after plugging in |

## Build and flash

```bash
pio run -t upload
```

This works for the first upload and for all later ones. The board switches to
download mode by itself when PlatformIO, esptool or the web flasher asks over the
serial port. After such a request the serial port reappears under a new name:
PlatformIO follows it, the web flasher may ask you to pick the port again.

Every start begins with a 10 s window (LED blinks fast) in which the chip's
built-in USB serial port is active and the network is not up yet. After that the
LED blinks slowly until the host has an address, then stays on.

## If the normal upload fails

The 10 s window needs none of this firmware's USB code, so it also works when the
serial port or the network does not. Build first, because the build takes longer
than the window:

```bash
pio run -e rescue
```

Then plug the board in and, while the LED blinks fast, run:

```bash
pio run -e rescue -t nobuild -t upload
```

The web flasher also works in the window. BOOT + RESET remains the last resort.

## Test

Try the board on a computer before it goes into a closed box:

1. Upload, then replug. A new network interface appears, and
   `http://192.168.7.1` opens the page.
2. Open the serial port at 115200 baud and send `PING`. It answers `PONG`.
3. Run `pio run -t upload` again. This is the button-free path.
4. Unplug, plug in, and run the two rescue steps above.

On the iPhone or iPad:

1. Unlock it, plug the board in with a USB-C data cable and allow the accessory if
   iOS asks. Lightning devices need a Lightning to USB 3 Camera Adapter with power.
2. Wait 10 s. Check that the LED stays on and that Settings shows **Ethernet**. Note
   any message iOS shows while the LED blinks fast.
3. Open `http://192.168.7.1` in Safari. It should show **Link OK**.
4. Lock the device for 1 minute, unlock it and return to Safari. Repeat for
   10 minutes.

| Page after unlocking | Meaning |
|---|---|
| Link OK, Board reboots 0 | The link survives sleep |
| Link OK, Board reboots above 0 | Link back, but iOS cut the power and the board restarted |
| Stays on "No reply" | The link does not survive sleep; unplug and replug to recover |

If iOS rejects the device, upload `network_only` the same way as `rescue` and repeat
the test. That tells a problem with the network apart from one with the combination.
Note the iOS version, the device model and which row you got.

Android: the ODMR tab of UC2 Control connects to the serial port and shows
`usb-test` as the firmware version.

## Limits

- The 10 s window delays every start. Shorten `RESCUE_WINDOW_S` in `src/main.c` once
  the button-free upload is proven on your boards, but only remove it if BOOT + RESET
  is reachable.
- The serial port only knows `PING` and `VERSION`, not the ODMR commands.
- Only CDC-NCM. `esp_tinyusb` has no default descriptors for CDC-ECM, so trying ECM
  needs custom descriptors.
- `esp_tinyusb` 3.0.0 ships TinyUSB 0.21.0~2. Its changelog lists a TinyUSB security
  fix for the next release; update before using this in a product.
- Built and partly tested off the board so far. Not yet run on hardware.
