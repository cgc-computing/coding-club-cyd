# ESP32 CYD Setup Guide

This guide walks through setting up one of our Coding Club **ESP32 CYD (Cheap Yellow Display)** boards from scratch using **Arduino IDE 2.x**.

The goal is to get from a brand-new computer and CYD board to a verified working display with the correct orientation.

These instructions are based on the specific boards we tested for Coding Club.

---

## Hardware We Are Using

Our boards are the dual-USB version of the:

**ESP32-2432S028**

Verified hardware:

- ESP32-D0WD-V3, revision 3.1
- 2.8-inch 240×320 TFT display
- USB-C programming connection
- ESP32 Wi-Fi and Bluetooth
- microSD slot
- resistive touchscreen
- TFT backlight on GPIO 21

The display on our boards requires a slightly different TFT_eSPI configuration than some older CYD tutorials.

---

# 1. Install Arduino IDE

Download and install the current **Arduino IDE 2.x** from:

https://www.arduino.cc/en/software

For a modern Mac with an Apple Silicon processor such as an M1, M2, M3, M4, or newer, choose the Apple Silicon version.

Move Arduino IDE into the Applications folder and launch it.

---

# 2. Install Rosetta on Apple Silicon Macs

Some Arduino command-line utilities may still require Apple's Rosetta compatibility layer.

If Arduino gives an error similar to:

> bad CPU type in executable

open Terminal and run:

`softwareupdate --install-rosetta`

After Rosetta finishes installing:

1. Quit Arduino IDE completely.
2. Reopen Arduino IDE.
3. Try compiling again.

You only need to do this once.

---

# 3. Install ESP32 Support

Arduino IDE does not include ESP32 support by default.

Open:

**Arduino IDE → Settings**

Find:

**Additional Boards Manager URLs**

Add:

`https://espressif.github.io/arduino-esp32/package_esp32_index.json`

Click **OK**.

Next open:

**Tools → Board → Boards Manager**

Search for:

**esp32**

Install:

**esp32 by Espressif Systems**

---

# 4. Connect the CYD

Connect the CYD to the computer using a USB data cable.

Our boards were somewhat picky about USB connections.

A setup that worked reliably was:

**USB-A → USB-C**

with the USB-A side connected through a dock.

Some USB-C-to-USB-C cables did not even power the board.

If the board receives power, you should normally see a red LED on the back.

If the board powers up but Arduino does not detect it:

- try another known-good data cable
- try another USB port
- unplug and reconnect the board
- check whether macOS detects the device under System Information → USB

---

# 5. Select the Board

In Arduino IDE select:

**Tools → Board → esp32 → ESP32 Dev Module**

The CYD does not need a special CYD entry in the Arduino board list.

Use the following settings:

| Setting | Value |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 115200 |
| CPU Frequency | 240 MHz |
| Flash Frequency | 80 MHz |
| Flash Mode | DIO |
| Flash Size | 4 MB |
| Partition Scheme | Default 4 MB with SPIFFS |
| PSRAM | Disabled |

## Important: Upload Speed

Use:

**115200**

We found that the faster default speed of **921600** could connect to the ESP32 but then fail while writing to flash.

115200 was reliable.

---

# 6. Select the Serial Port

Go to:

**Tools → Port**

Choose the new USB serial device.

On macOS it will usually look something like:

`/dev/cu.usbserial-110`

The exact number may change after reconnecting the board.

For example, the same board may later appear as:

`/dev/cu.usbserial-140`

That is normal.

If Arduino says:

> no upload port provided

go back to **Tools → Port** and select the CYD again.

---

# 7. Install TFT_eSPI

Open:

**Sketch → Include Library → Manage Libraries**

Search for:

**TFT_eSPI**

Install:

**TFT_eSPI by Bodmer**

Our tested version was:

**TFT_eSPI 2.5.43**

---

# 8. Configure TFT_eSPI for Our CYD

TFT_eSPI must be configured for the exact display wiring on the board.

Open the TFT_eSPI library folder.

On a typical macOS Arduino installation this is:

`~/Documents/Arduino/libraries/TFT_eSPI/`

Open:

`User_Setup.h`

Configure it for the following hardware:

| Function | GPIO |
|---|---:|
| TFT MISO | 12 |
| TFT MOSI | 13 |
| TFT Clock | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT Reset | Not used / -1 |
| TFT Backlight | 21 |

Use:

- **ILI9341_2** display driver
- **27 MHz SPI frequency**
- **HSPI**
- backlight active **HIGH**

This is important.

Using the regular **ILI9341** driver caused only part of the display to update. The remaining part of the screen stayed static.

Changing to **ILI9341_2** fixed the problem and allowed the full screen to update correctly.

Also verify that `User_Setup_Select.h` is using `User_Setup.h` and not one of the other predefined TFT_eSPI setup files.

---

# 9. Verify the TFT Configuration

Before testing graphics, it is useful to open the TFT_eSPI diagnostic example:

**File → Examples → TFT_eSPI → Test and diagnostics → Read_User_Setup**

Upload it to the CYD.

Then open:

**Tools → Serial Monitor**

Set the Serial Monitor to:

**115200 baud**

Press the **RST** button on the back of the CYD.

The configuration should report approximately:

| Setting | Expected |
|---|---:|
| Driver | ILI9341_2 |
| MOSI | 13 |
| MISO | 12 |
| SCK | 14 |
| TFT_CS | 15 |
| TFT_DC | 2 |
| TFT_RST | -1 |
| TFT_BL | 21 |
| SPI Frequency | 27 MHz |

If these values are wrong, fix the TFT_eSPI configuration before continuing.

---

# 10. Upload the Display Verification Sketch

Create a new Arduino sketch and replace its contents with the following.

```cpp
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.init();

  // Required for our CYD display
  tft.invertDisplay(true);

  // Landscape orientation: 320 x 240
  tft.setRotation(1);

  // Test full-screen colors
  tft.fillScreen(TFT_RED);
  delay(1000);

  tft.fillScreen(TFT_GREEN);
  delay(1000);

  tft.fillScreen(TFT_BLUE);
  delay(1000);

  tft.fillScreen(TFT_WHITE);
  delay(1000);

  // Final screen
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(
    "CODING CLUB CYD",
    tft.width() / 2,
    tft.height() / 2 - 20,
    4
  );

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(
    "DISPLAY SETUP OK",
    tft.width() / 2,
    tft.height() / 2 + 25,
    2
  );
}

void loop() {
}
```

---

# 11. What You Should See

After uploading, the entire display should show:

1. Red
2. Green
3. Blue
4. White
5. Black background with:

**CODING CLUB CYD**

and

**DISPLAY SETUP OK**

The text should be upright with the display in landscape orientation.

The usable resolution in this orientation is:

**320 × 240**

---

# 12. Troubleshooting

## Screen is completely white

First verify the TFT_eSPI configuration.

Check:

- correct GPIO pins
- correct setup file
- correct display driver
- SPI frequency

The display backlight can work even when communication with the TFT controller is not working.

---

## Only part of the screen updates

This was an important issue on our boards.

If roughly 240 pixels update while the remaining part of the screen stays unchanged, make sure the TFT_eSPI driver is:

**ILI9341_2**

and not:

**ILI9341**

The regular ILI9341 driver did not correctly address the entire display on our CYD revision.

---

## Display is rotated 90 degrees

Make sure the sketch uses:

**Rotation 1**

This gives us the desired:

**320 × 240 landscape orientation**

---

## Upload starts but fails

Check the upload speed.

Use:

**115200**

Our boards were unreliable at:

**921600**

---

## CYD has power but no serial port appears

Try:

- another USB cable
- another USB port
- unplugging and reconnecting the board
- USB-A-to-USB-C instead of USB-C-to-USB-C
- checking System Information → USB on macOS

A red LED only confirms that the board is receiving power. It does not guarantee that the USB cable carries data.

---

# 13. Known-Good CYD Configuration

Keep this information handy when working on Coding Club projects.

| Item | Working Configuration |
|---|---|
| Board | ESP32-2432S028 dual-USB CYD |
| ESP32 | ESP32-D0WD-V3 rev 3.1 |
| Arduino Board | ESP32 Dev Module |
| Upload Speed | 115200 |
| Display | 2.8-inch 240×320 |
| Landscape Resolution | 320×240 |
| TFT Driver | ILI9341_2 |
| Rotation | 1 |
| Invert Display | Enabled |
| TFT MISO | GPIO 12 |
| TFT MOSI | GPIO 13 |
| TFT Clock | GPIO 14 |
| TFT CS | GPIO 15 |
| TFT DC | GPIO 2 |
| TFT Reset | -1 |
| Backlight | GPIO 21 |
| Backlight Level | HIGH |
| SPI Frequency | 27 MHz |

---

# 14. You're Ready

If the verification sketch:

- uploads successfully
- fills the entire screen
- shows the correct colors
- displays the final message
- and is correctly oriented in landscape

then your CYD is ready for Coding Club projects.

Three sketches in this repository are ready to run:

- **[generative_landscape](../sketches/generative_landscape/)** — a new
  procedural mountain scene on every button press. Display only.
- **[CYDSolarSystem](../sketches/CYDSolarSystem/)** — an animated orrery. Display only,
  and a good introduction to flicker-free sprite rendering.
- **[WiFiDetectiveDeluxe](../sketches/WiFiDetectiveDeluxe/)** — a Wi-Fi analyzer. This
  is the one that uses the touchscreen, so start here if you want to verify
  touch works on your board.

Other ideas the club could take on: dice and game simulations, sensor
dashboards, touch-based games, clocks and timers, and retro-style demos.

## A note on the touchscreen

This guide verifies the **display** only. The touchscreen is a separate
XPT2046 controller on its own SPI bus (T_CLK 25, T_CS 33, T_DIN 32, T_OUT 39,
T_IRQ 36) — it does **not** share the display's pins, and TFT_eSPI's built-in
touch support assumes it does, so that support is not used.

Touch also needs calibrating per board. `WiFiDetectiveDeluxe` includes a
four-corner calibration routine that saves to flash and runs automatically the
first time it boots.

The important first milestone is always the same:

**Get the hardware working reliably before building the application.**
