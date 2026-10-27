# Hosyond 4.0-Inch ESP32 Touchscreen Setup Guide

**Coding Club • Windows-first setup, with macOS and Raspberry Pi/Linux instructions**  
**Device:** Hosyond 4.0-inch 320 × 480 ESP32-32E touchscreen (Amazon ASIN **B0FGJJ24S1**)  
**Purchase link:** https://www.amazon.com/dp/B0FGJJ24S1

> This is a **separate board** from our 2.8-inch ESP32-2432S028 CYD. Do not copy the 2.8-inch display driver or backlight configuration. See [CYD-SETUP.md](CYD-SETUP.md) for the smaller board.
>
> These settings were used successfully on a Hosyond 4-inch board with an Apple Silicon Mac: USB flashing, ST7796S full-screen graphics, XPT2046 touch readings, touch calibration, and a small paint app. The Windows and Raspberry Pi instructions use the same Arduino sketches and hardware configuration, but have not been separately tested for this particular board.

## What we are building

By the end, you should be able to:

1. Connect the board to a Windows PC (or Mac/Raspberry Pi).
2. Upload code with Arduino IDE 2.x.
3. See diagnostic output in Serial Monitor.
4. Drive the entire 4-inch TFT in landscape orientation (480 × 320).
5. Read raw touchscreen coordinates.
6. Calibrate the touch controller and save the result in nonvolatile storage.
7. Draw on the screen using a simple paint application.

---

## 1. Hardware and product details

| Feature | Hosyond 4-inch board |
|---|---|
| Product name | Hosyond 4.0-inch 320×480 Touch Screen with ESP32-32E |
| Amazon | [B0FGJJ24S1](https://www.amazon.com/dp/B0FGJJ24S1) |
| MCU/module | ESP32-32E / ESP32-WROOM-32E family |
| Observed processor on tested board | **ESP32-D0WD-V3, revision 3.1** |
| Processor | Dual-core Xtensa LX6, up to 240 MHz |
| Wireless | 2.4 GHz 802.11 b/g/n Wi-Fi; Bluetooth 4.2 BR/EDR + BLE |
| External flash | 4 MB (manufacturer specification; verify your individual board) |
| On-chip SRAM | 520 KB (chip specification; not all available to an Arduino sketch) |
| Display | 4.0-inch color TFT, 320 × 480 pixels in portrait |
| Landscape drawing area | **480 × 320** when using `setRotation(1)` |
| LCD controller | **ST7796S** over SPI |
| Touch | **XPT2046** resistive touchscreen over SPI |
| Backlight | GPIO **27**, HIGH = on |
| Connector for programming | USB-C |
| Additional board features | microSD slot, expansion/serial interfaces, speaker interface, battery connection/charging circuitry, and RGB LED (availability/usage should be verified on your revision) |
| Approximate board size | 111.1 × 60.9 × 5.7 mm |
| Typical kit contents | Display board, USB cable, touch stylus, 4-pin lead; check your package |

**Important:** It is an ESP32 development board with a built-in display—not a stand-alone HDMI or USB monitor. A Raspberry Pi can be used **as the development computer**, but the display is not a plug-and-play Raspberry Pi monitor.

**Why a stylus?** The XPT2046 screen is *resistive*, not capacitive: apply light physical pressure rather than relying on proximity or multitouch gestures.

### Confirmed GPIO map

| Device | Signal | ESP32 GPIO |
|---|---|---|
| LCD | MOSI | 13 |
| LCD | MISO | 12 |
| LCD | SCLK | 14 |
| LCD | CS | 15 |
| LCD | DC | 2 |
| LCD | RST | -1 (not separately driven) |
| LCD | Backlight | **27** |
| Touch | CS | **33** |
| Touch | IRQ | **36** (optional; not needed for our polling examples) |
| Touch | MOSI/MISO/SCLK | **Shared with LCD**: 13 / 12 / 14 |

> **Contrast with the 2.8-inch CYD:** That board uses a different LCD driver/backlight pin, and on our 2.8-inch revision the touch controller has a different SPI wiring arrangement. These guides are *not* interchangeable.

---

## 2. Prepare the computer

### Windows 10/11 (primary instructions)

1. Download **Arduino IDE 2.x** from https://www.arduino.cc/en/software.
2. Install it and open it.
3. Plug the Hosyond into a **USB data cable** (some charging-only cables do not transmit data).
4. Open **Device Manager → Ports (COM & LPT)**. Look for a new `USB-SERIAL` or similar device and write down the COM number, e.g. `COM5`.
5. If no COM port appears, try a different cable, a direct USB connection, or another port. Under **Other devices**, an unknown USB serial adapter may indicate a missing USB-to-UART driver. Identify the chip in Device Manager (hardware IDs) or on the board before installing the appropriate manufacturer's driver (commonly CH340/CH341 or CP210x). Do not install random driver bundles.

### macOS (Apple Silicon or Intel)

1. Download the correct macOS version of Arduino IDE 2.x.
2. Connect the board and run in Terminal:

   ```bash
   ls /dev/cu.*
   ```

3. Look for a new name such as `/dev/cu.usbserial-210`. The suffix can change after unplugging/reconnecting.
4. **Do not select `/dev/cu.debug-console`**; that is not the Hosyond board.
5. If Arduino reports `bad CPU type in executable` for an older tool, install Rosetta only if necessary:

   ```bash
   softwareupdate --install-rosetta
   ```

6. If a serial port is busy, close Arduino Serial Monitor/Plotter or any other program holding the port. `lsof /dev/cu.usbserial-210` can help identify it (substitute your current port).

### Raspberry Pi OS / other Linux (optional)

Arduino IDE 2.x support depends on the Pi's **CPU architecture** and the available Arduino/Espressif binaries. On a **64-bit ARM Raspberry Pi**, Arduino IDE 2.x may not be available as an officially supported native package. Prefer a supported x86-64 Linux computer for the graphical IDE, or use **Arduino CLI** on the Pi if compatible builds and ESP32 toolchain binaries are available. Another option is compiling on a PC/Mac and programming the board there.

For serial-port discovery on Raspberry Pi OS:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
# Plug in the board, then compare:
dmesg | tail -n 25
```

A typical board appears as `/dev/ttyUSB0`; some USB adapters use `/dev/ttyACM0`. Add your user to `dialout` if needed, then **log out and back in**:

```bash
sudo usermod -aG dialout "$USER"
```

Do **not** connect the board's USB-C programming port to Raspberry Pi GPIO pins. Use an ordinary USB data connection. The sketches below are the same on every host OS.

---

## 3. Install ESP32 support in Arduino IDE

1. Open **File → Preferences** on Windows/Linux, or **Arduino IDE → Settings** on macOS.
2. Under **Additional Boards Manager URLs**, add:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

3. Open **Boards Manager**, search `esp32`, and install **esp32 by Espressif Systems**.
4. Select **Tools → Board → esp32 → ESP32 Dev Module**.
5. Select **Tools → Port** and choose the board's `COMx`, `/dev/cu.usbserial-*`, or `/dev/ttyUSB*` port.

### Known-good initial settings

| Setting | Value |
|---|---|
| Board | ESP32 Dev Module |
| Upload speed | **115200** (important) |
| CPU frequency | 240 MHz |
| Flash frequency | 40 MHz |
| Flash mode | DIO |
| Flash size | 4 MB, if your board reports 4 MB |
| PSRAM | Disabled unless you confirm your variant includes it |
| Partition scheme | Default |
| Serial Monitor baud rate | 115200 |

**Important:** On the tested board, the initial 921600-baud upload connected but failed during flashing. Using **115200 baud** was a reliable starting point. Upload speed and Serial Monitor speed are independent settings.

---

## 4. First upload: serial-only bring-up

Create a sketch named `Hosyond_Bringup` and paste:

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("HOSYOND ESP32 BRING-UP");
  Serial.printf("Chip: %s\n", ESP.getChipModel());
  Serial.printf("CPU: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
}

void loop() {
  static unsigned long count = 0;
  Serial.printf("Running! Count: %lu\n", count++);
  delay(2000);
}
```

Click **Upload**. Successful flashing usually reports `Hash of data verified` and `Hard resetting via RTS pin`. Open **Serial Monitor** at 115200 baud. You should see a count increasing every two seconds.

**A blank LCD at this stage is expected:** this sketch never initializes the screen. Uploading any sketch can replace a pre-installed factory demo.

If upload hangs at `Connecting...`, hold **BOOT**, start the upload, then release BOOT when writing begins. Press **EN/RESET** afterward if necessary.

---

## 5. Install TFT_eSPI

Open **Tools → Manage Libraries**, search for **TFT_eSPI by Bodmer**, and install it.

**Important:** TFT_eSPI normally uses a *library-level* configuration file. Each computer must be configured separately; it is **not enough** to transfer just a sketch between computers.

Find your Arduino sketchbook's `libraries/TFT_eSPI` folder. Typical locations:

- Windows: `Documents\Arduino\libraries\TFT_eSPI\`
- macOS: `~/Documents/Arduino/libraries/TFT_eSPI/`
- Linux: `~/Arduino/libraries/TFT_eSPI/`

Locations can differ if you changed **Sketchbook location** in IDE preferences.

### 5A. Create a dedicated setup file

Create:

`TFT_eSPI/User_Setups/Setup_Hosyond_4in.h`

with:

```cpp
#define USER_SETUP_INFO "Hosyond 4-inch ESP32"

#define ST7796_DRIVER
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

#define TFT_BL   27
#define TFT_BACKLIGHT_ON HIGH

#define TOUCH_CS 33
#define SPI_TOUCH_FREQUENCY 2500000

#define USE_HSPI_PORT
#define SPI_FREQUENCY      27000000
#define SPI_READ_FREQUENCY 6000000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
```

### 5B. Select the setup

Open `TFT_eSPI/User_Setup_Select.h`:

1. Comment out the default include, typically:

   ```cpp
   // #include <User_Setup.h>
   ```

2. Add or uncomment **one** active custom setup include:

   ```cpp
   #include <User_Setups/Setup_Hosyond_4in.h>
   ```

3. Ensure no other display setup is selected. Save both files.

If you also own the 2.8-inch CYD, remember that **changing this setup switches TFT_eSPI for all sketches compiled with this library**. Consider keeping separate library copies or project-local configurations when regularly switching boards. Library updates can overwrite manually edited configuration files.

### 5C. Optional diagnostics

Open **File → Examples → TFT_eSPI → Test and diagnostics → Read_User_Setup** and check the Serial Monitor at 115200. It should report ST7796, the defined pins, HSPI and an approximately 27 MHz display SPI clock. If it reports ILI9341, you're using the wrong setup file.

---

## 6. LCD graphics test

Create `Hosyond_Display_Test`:

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

void showColor(uint16_t color, const char* name) {
  tft.fillScreen(color);
  tft.setTextColor(TFT_WHITE, color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(name, tft.width() / 2, tft.height() / 2, 4);
  Serial.println(name);
  delay(1500);
}

void setup() {
  Serial.begin(115200);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  tft.init();
  tft.setRotation(1);  // 480 x 320 landscape
  Serial.printf("Display: %d x %d\n", tft.width(), tft.height());
}

void loop() {
  showColor(TFT_RED, "RED");
  showColor(TFT_GREEN, "GREEN");
  showColor(TFT_BLUE, "BLUE");
  showColor(TFT_BLACK, "BLACK");
}
```

**Expected:** The full display changes red → green → blue → black, with centered labels. The serial output should report **480 x 320**. If successful, the LCD, SPI configuration, backlight, and landscape rotation are working.

---

## 7. Test the touchscreen (raw readings)

Unlike our particular 2.8-inch CYD, this 4-inch board's XPT2046 **shares the LCD SPI bus**. The `TOUCH_CS 33` definition enables TFT_eSPI's built-in polling routines; no separate touchscreen library is needed for this guide.

Create `Hosyond_Touch_Test`:

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

void setup() {
  Serial.begin(115200);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("HOSYOND TOUCH TEST", 20, 20, 4);
  tft.drawString("Touch the screen", 20, 65, 2);
}

void loop() {
  uint16_t x, y;
  uint16_t pressure = tft.getTouchRawZ();
  if (pressure > 350) {
    tft.getTouchRaw(&x, &y);
    Serial.printf("Raw X=%u Y=%u pressure=%u\n", x, y, pressure);
    tft.fillRect(10, 110, 460, 120, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("X: " + String(x), 20, 110, 4);
    tft.drawString("Y: " + String(y), 20, 160, 4);
  }
  delay(100);
}
```

**Expected:** Touches produce changing raw X/Y values in Serial Monitor. The raw numbers **do not yet correspond** to 480 × 320 screen pixels; calibration handles that.

---

## 8. Calibrate touch and save the calibration

**Calibration is per physical board and screen orientation.** We'll use TFT_eSPI's four-corner calibration and the built-in ESP32 `Preferences` library for nonvolatile storage (NVS).

Create `Hosyond_Touch_Calibration`:

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>

TFT_eSPI tft;
Preferences prefs;
uint16_t cal[5];

void setup() {
  Serial.begin(115200);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  tft.init();
  tft.setRotation(1);

  prefs.begin("hosyond", false);
  if (prefs.getBytesLength("touchCal") == sizeof(cal)) {
    prefs.getBytes("touchCal", cal, sizeof(cal));
    Serial.println("Loaded saved calibration");
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Touch corner targets", 20, 40, 2);
    tft.calibrateTouch(cal, TFT_MAGENTA, TFT_BLACK, 15);
    prefs.putBytes("touchCal", cal, sizeof(cal));
    Serial.println("Saved calibration");
  }
  tft.setTouch(cal);
  prefs.end();

  Serial.printf("Cal: %u, %u, %u, %u, %u\n",
                cal[0], cal[1], cal[2], cal[3], cal[4]);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("CALIBRATED - TOUCH TO TEST", 8, 20, 2);
}

void loop() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y, 350)) {
    Serial.printf("Screen X=%u Y=%u\n", x, y);
    tft.fillCircle(x, y, 3, TFT_YELLOW);
    delay(35);
  }
}
```

On the first run, tap the indicated corner targets carefully with the stylus. The results are saved under the Preferences namespace `hosyond`, key `touchCal`. Subsequent resets should reuse calibration. **Do not change `setRotation(1)` without recalibrating.**

To **force recalibration** using a sketch, open `Preferences` with the same namespace and call `remove("touchCal")` once, or erase that key in your application before entering calibration. Do **not** erase the entire flash merely to recalibrate. If you change board/partition layout or erase all flash, saved NVS values may be lost.

---

## 9. Make a simple drawing app

This app provides six color buttons, +/− brush size, CLEAR, and continuous strokes. It uses the saved calibration from section 8.

Create `Hosyond_Paint`:

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>

TFT_eSPI tft;
Preferences prefs;

const int W = 480, H = 320;
const int BAR_H = 54, DRAW_TOP = 58, DRAW_BOTTOM = 289;
const uint16_t PALETTE[6] = {
  TFT_RED, TFT_GREEN, TFT_BLUE,
  TFT_YELLOW, TFT_MAGENTA, TFT_WHITE
};

int colorIndex = 0;
int brush = 4;
bool previousTouch = false;
bool strokeActive = false;
int lastX = 0, lastY = 0;

void toolbar() {
  tft.fillRect(0, 0, W, BAR_H, TFT_DARKGREY);
  for (int i = 0; i < 6; i++) {
    int x = 3 + 54*i;
    tft.fillRoundRect(x, 6, 48, 40, 5, PALETTE[i]);
    if (i == colorIndex) tft.drawRoundRect(x, 6, 48, 40, 5, TFT_CYAN);
  }
  tft.fillRoundRect(326, 6, 40, 40, 5, TFT_NAVY);
  tft.fillRoundRect(370, 6, 40, 40, 5, TFT_NAVY);
  tft.fillRoundRect(414, 6, 63, 40, 5, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("-", 346, 26, 4);
  tft.drawString("+", 390, 26, 4);
  tft.drawString("CLEAR", 445, 26, 2);
}

void statusBar() {
  tft.fillRect(0, DRAW_BOTTOM, W, H - DRAW_BOTTOM, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("Brush: " + String(brush) + " px", 10, 296, 2);
  tft.drawString("HOSYOND PAINT", 315, 296, 2);
}

void clearCanvas() {
  tft.fillRect(0, DRAW_TOP, W, DRAW_BOTTOM-DRAW_TOP, TFT_BLACK);
}

void dot(int x, int y) {
  if (x >= brush && x < W-brush &&
      y >= DRAW_TOP+brush && y < DRAW_BOTTOM-brush) {
    tft.fillCircle(x, y, brush, PALETTE[colorIndex]);
  }
}

void lineStroke(int x0, int y0, int x1, int y1) {
  int dx = x1-x0, dy = y1-y0;
  int steps = max(abs(dx), abs(dy));
  if (!steps) { dot(x1, y1); return; }
  for (int i = 0; i <= steps; i++) {
    int x = x0 + (int)((int32_t)dx*i / steps);
    int y = y0 + (int)((int32_t)dy*i / steps);
    dot(x, y);
  }
}

void calibrateOrLoad() {
  uint16_t data[5];
  prefs.begin("hosyond", false);
  if (prefs.getBytesLength("touchCal") == sizeof(data)) {
    prefs.getBytes("touchCal", data, sizeof(data));
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.calibrateTouch(data, TFT_MAGENTA, TFT_BLACK, 15);
    prefs.putBytes("touchCal", data, sizeof(data));
  }
  tft.setTouch(data);
  prefs.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  tft.init();
  tft.setRotation(1);
  calibrateOrLoad();
  tft.fillScreen(TFT_BLACK);
  toolbar(); clearCanvas(); statusBar();
}

void loop() {
  uint16_t x, y;
  bool touched = tft.getTouch(&x, &y, 350);
  if (touched) {
    if (!previousTouch) {
      strokeActive = false;
      if (y < BAR_H) {
        if (x < 324) {
          int i = x/54;
          if (i >= 0 && i < 6) { colorIndex = i; toolbar(); }
        } else if (x >= 326 && x < 366) {
          brush = max(1, brush-1); statusBar();
        } else if (x >= 370 && x < 410) {
          brush = min(15, brush+1); statusBar();
        } else if (x >= 414) clearCanvas();
      } else if (y >= DRAW_TOP && y < DRAW_BOTTOM) {
        dot(x, y);
        lastX = x; lastY = y;
        strokeActive = true;
      }
    } else if (strokeActive) {
      if (y >= DRAW_TOP && y < DRAW_BOTTOM) {
        lineStroke(lastX, lastY, x, y);
        lastX = x; lastY = y;
      } else strokeActive = false;
    }
  } else strokeActive = false;
  previousTouch = touched;
  delay(10);
}
```

**Try it:** Change colors, draw a line across the black area, adjust brush size, and press CLEAR. The *drawing* is not saved after a reset; only the *touch calibration* is persistent.

---

## 10. Troubleshooting

| Symptom | Things to check |
|---|---|
| No COM/USB serial port | Use a known-good **data** cable; try direct USB; check Device Manager / macOS `ls /dev/cu.*` / Linux `dmesg`; identify USB-UART adapter before installing drivers. |
| macOS selects `/dev/cu.debug-console` | Wrong port. Choose the `/dev/cu.usbserial-*` entry that appeared after connecting the board. |
| `Failed to connect: No serial data received` | Verify port, cable, power, USB serial driver, and BOOT-button upload procedure. |
| Upload gets as far as 921600 baud, then fails verifying flash | Lower **Tools → Upload Speed** to **115200**; retry with direct USB and a different cable. |
| `Resource busy` or serial terminal cannot open | Close Serial Monitor, Plotter, `screen`, or other software already holding the port. |
| Garbled boot characters | Check Serial Monitor baud 115200, press EN, and confirm the **sketch upload actually succeeded**. Repeated corrupted messages can also indicate a flash/startup issue. |
| Blank LCD after serial-only sketch | Expected: serial test does **not** initialize the LCD. |
| Backlight on but screen blank/white with graphics sketch | Confirm `ST7796_DRIVER`, `USE_HSPI_PORT`, actual active setup file, all display pins, and GPIO27 HIGH. |
| Only part of screen updates / odd colors | Recheck **ST7796** versus ILI9341, rotation, SPI wiring, and selected TFT_eSPI setup. |
| Touch doesn't respond | Verify `TOUCH_CS 33`, 2.5 MHz touch SPI speed, shared SPI pins, and gentle pressure; touch is resistive. |
| Touch locations are wrong | Run corner calibration; keep `setRotation(1)` consistent. |
| Touch calibration repeats on every boot | Check Preferences namespace/key and NVS availability; do not use an erase-all-flash upload setting. |
| `TFT_eSPI.h: No such file or directory` | Install TFT_eSPI from Library Manager; confirm Arduino's sketchbook location. |
| Colors/text work but paint doesn't | Confirm touch works in raw test and `getTouch()` receives calibrated values. |
| Sketch stops working after TFT_eSPI update | Recheck `User_Setup_Select.h`: manual library changes may have been overwritten. |

**Diagnostic principle:** Test in this order: **USB serial → successful flash → serial counter → display → raw touch → calibrated touch → app**. Fix one layer at a time.

---

## 11. Known-good configuration summary

| Item | Working value |
|---|---|
| Product | Hosyond 4-inch ESP32-32E, [Amazon B0FGJJ24S1](https://www.amazon.com/dp/B0FGJJ24S1) |
| Observed ESP32 | ESP32-D0WD-V3 revision 3.1 |
| Arduino board | ESP32 Dev Module |
| Upload speed | 115200 |
| Tested host | Apple Silicon Mac; Windows instructions above; Raspberry Pi/Linux notes provided |
| LCD | 4-inch, 320×480 TFT |
| Landscape | 480×320 with `setRotation(1)` |
| LCD driver | `ST7796_DRIVER` |
| SPI controller | HSPI |
| MISO / MOSI / SCLK | 12 / 13 / 14 |
| TFT CS / DC / RST | 15 / 2 / -1 |
| LCD backlight | GPIO27 HIGH |
| Display SPI | 27 MHz |
| Touch driver | XPT2046 built-in TFT_eSPI support |
| Touch CS / IRQ | 33 / 36 (IRQ not used in guide) |
| Touch SPI | 2.5 MHz |
| Calibration | `calibrateTouch` + `Preferences` namespace `hosyond`, key `touchCal` |

---

## 12. Helpful links and next projects

- [Hosyond Amazon listing — B0FGJJ24S1](https://www.amazon.com/dp/B0FGJJ24S1)
- [Our original 2.8-inch CYD setup guide](CYD-SETUP.md)
- [Arduino IDE downloads](https://www.arduino.cc/en/software)
- [Espressif Arduino-ESP32 installation guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [Espressif esptool troubleshooting](https://docs.espressif.com/projects/esptool/en/latest/esp32/troubleshooting.html)
- [TFT_eSPI library](https://github.com/Bodmer/TFT_eSPI)
- [Manufacturer-style product specifications and feature description](https://manuals.plus/asin/B0FGJJ24S1)
- [Hosyond-specific pin-map reference](https://github.com/johannstark/hosyond-esp32-4in-screen-reference)

With a calibrated 480×320 screen you can build a touchscreen game, menu-based Wi-Fi scanner, digital dashboard, on-screen keyboard, or drawing application. The 4-inch board is a useful upgrade from the 2.8-inch CYD, but always check the board-specific pins before porting a sketch.

**Coding Club takeaway:** Bring up the hardware in stages; save the known-good settings; only then start building the fun stuff.
