# DevBoxSDK

Hardware abstraction library for the DevBox portable console based on ESP32-S3.

## Overview

DevBoxSDK provides an Arduino-friendly interface for the custom DevBox handheld hardware.  
It is intended to simplify development by wrapping display, controls, audio, SD card, and system-level functionality into a single library.

## Features

- ESP32-S3 target platform

- SSD1363 256x128 OLED support

- 4-bit grayscale framebuffer

- U8G2 integration with custom driver callback

- Button input handling

- Audio support

- SD card helpers

- System helpers

## Design goals

  - Provide a minimal and fast abstraction over hardware
  - Avoid unnecessary overhead for real-time applications
  - Keep API simple and Arduino-friendly
  - Allow direct access when needed (no heavy abstraction lock-in)

## Target hardware

This SDK is designed for the DevBox handheld hardware:
- ESP32-S3
- SSD1363 256x128 OLED
- MAX98357A (I2S DAC / amplifier)
- 8080 Parallel display interface
- Custom hardware abstraction

## Repository structure

- `src/` — library source code
- `examples/` — sample Arduino sketches
- `library.properties` — Arduino library metadata

## Installation

### Arduino IDE

1. Download or clone this repository.

2. Place it in your Arduino `libraries` folder.

3. Restart Arduino IDE.

4. Select `ESP32S3 Dev Module` board 

5. Configure required options (USB, OTA, etc.):

   * USB CDC On Boot: "Enabled"

   * USB DFU On Boot: "Disabled"

   * Flash Size: "4MB (32 Mb)"

   * JTAG Adapter: "Integrated USB JTAG"

   * USB Firmware MSC On Boot: "Disabled"

   * Upload Mode: "UART0 / Hardware CDC"

   * USB Mode: "Hardware CDC and JTAG"

   * Zigbee Mode: "Disabled"

     These options are necessary in order for Serial, OTA and USB file transfer to work properly. 

6. Build your sketch.

7. Upload it directly or copy to your SD card /apps folder.

## Quick start

### Option 1 — Direct SDK usage

Use `examples/test_app` as a starting point. 

### Option 2 — DevBoxEngine

Use the engine layer for structured game development (recommended for larger projects).

## Modules

### Display

Graphics and test output.

### Input

Button polling and pressed-state helpers.

### Audio

Audio playback/output helpers.

### SD card

File access for assets and data.

## Project status

This project is under active development and may change as the hardware and software evolve.

## Limitations

- Hardware-specific (not portable to generic ESP32 setups)
- API may change during development
- Some modules are still experimental

## Contributing

Contributions, bug reports, and suggestions are welcome.

