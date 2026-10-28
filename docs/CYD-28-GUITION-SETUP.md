# Guition / DIYmalls 2.8-inch CYD — ESP32-2432S028

**CGCC Coding Club — Hardware Setup Guide**  
**Status:** Display and touchscreen tested successfully on our board (October 2026)

## Product and identification

- **Amazon:** [DIYmalls 2.8-inch ESP32-2432S028R](https://www.amazon.com/dp/B0D8W9DSYZ) (ASIN **B0D8W9DSYZ**)
- **Seller/listing name:** DIYmalls ESP32-2432S028R, 2.8-inch resistive-touch TFT, with case advertised in some listing versions.
- **Board markings on our unit:** Guition; ESP32-2432S028; TF V1.4.
- **Nickname:** CYD, or *Cheap Yellow Display*.

> **Variant warning:** Different boards sold as “CYD” can have different displays, touch controllers, and USB arrangements. This guide records settings **tested on our particular unit**. Do not assume that configurations for the 4-inch Hosyond board or other CYDs are interchangeable.

## Hardware overview

| Feature | Details |
|---|---|
| Microcontroller | ESP32-D0WD-V3, revision 3.1 (confirmed in upload log) |
| Wireless | ESP32 Wi-Fi and Bluetooth |
| Display | 2.8-inch color TFT, 240 × 320 portrait / 320 × 240 landscape |
| Display library driver | **`ILI9341_2_DRIVER`** in TFT_eSPI (**verified by color test**) |
| Touchscreen | Resistive touch, XPT2046 (**verified by touch test**) |
| Display backlight | GPIO 21, active HIGH (**verified**) |
| Storage | MicroSD/TF slot present; not yet tested |
| USB | USB-C and Micro-USB connectors visible on our board; use the port detected by Arduino IDE |
| Other features | BOOT and RST buttons, RGB LED, speaker connector, expansion connectors (not yet tested) |
| Flash / PSRAM | Common versions have 4 MB flash and no PSRAM; verify the exact unit before relying on these specifications |

## Requirements

- Arduino IDE 2.x (Windows, macOS, or Linux)
- Espressif **esp32** board package installed in Arduino IDE Boards Manager
- Libraries installed using Library Manager:
  - **TFT_eSPI** (Bodmer)
  - **XPT2046_Touchscreen** (Paul Stoffregen)
- USB **data** cable and a suitable USB port
- This repository's sample sketches and display setup file

## Arduino IDE settings

Select **Tools → Board → ESP32 Dev Module** and the serial port corresponding to the board.

| Setting | Starting value |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | **115200** (tested, reliable) |
| Flash Mode | DIO (recommended starting point) |
| Flash Frequency | 40 MHz (recommended starting point) |
| Serial Monitor baud rate | **115200** |

**IMPORTANT:** On our Mac, an upload at **921600** failed with `Unable to verify flash chip connection`. Lowering **Upload Speed** to **115200** fixed the upload problem. Upload Speed and Serial Monitor baud rate are **different settings**.

A successful upload should complete without `Failed uploading: exit status 2`. If a sketch doesn't appear to run, inspect the **entire upload log before debugging the display**.

## Working TFT_eSPI display configuration

TFT_eSPI normally compiles its selected display setup into every sketch. If you have previously used another board (for example our 4-inch Hosyond 320 × 480 display), you must **switch to this CYD setup**.

Create `TFT_eSPI/User_Setups/Setup_CGCC_CYD_28.h` containing:

```cpp
#define USER_SETUP_INFO "CGCC CYD 2.8-inch Guition"
#define ILI9341_2_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#define SPI_FREQUENCY 27000000
```

In `TFT_eSPI/User_Setup_Select.h`, ensure **exactly one setup is selected** and point it to:

```cpp
#include <User_Setups/Setup_CGCC_CYD_28.h>
```

Comment out the default `#include <User_Setup.h>` and any other active setup includes. Do not also define another display driver. If your installation uses custom compiler flags or `USER_SETUP_LOADED`, make sure they do not override this file.

**Display pinout (tested)**

| Display signal | ESP32 GPIO |
|---|---:|
| MOSI | 13 |
| MISO | 12 |
| SCLK | 14 |
| CS | 15 |
| DC | 2 |
| RST | -1 (no separate GPIO) |
| Backlight | 21 |

### Display smoke test

```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft;

void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_RED);   delay(1000);
  tft.fillScreen(TFT_GREEN); delay(1000);
  tft.fillScreen(TFT_BLUE);  delay(1000);
  Serial.println("Display test complete");
}
void loop() {}
```

**Expected:** screen visibly changes **red → green → blue**. Serial messages alone do **not** prove the LCD is communicating.

We initially saw a white screen with an ST7789 configuration. Switching to **`ILI9341_2_DRIVER`** made the colors appear. We also discovered that a prior 4-inch display setup was still selected: its diagnostics reported driver `0x7796`, width `320`, height `480`. Always check which setup TFT_eSPI is actually compiling.

## Working XPT2046 touchscreen configuration

Our touchscreen test successfully reported changing raw X/Y readings using these pins:

| Touch signal | ESP32 GPIO |
|---|---:|
| CLK | 25 |
| MOSI | 32 |
| MISO | 39 |
| CS | 33 |
| IRQ | 36 |

Touch uses **a separate SPI bus** from the display. Initialize it explicitly:

```cpp
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(33, 36);

void setup() {
  Serial.begin(115200);
  touchSPI.begin(25, 39, 32, 33); // SCLK, MISO, MOSI, CS
  ts.begin(touchSPI);
  ts.setRotation(0);
  Serial.println("Touch test ready");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.printf("X=%d Y=%d Pressure=%d\n", p.x, p.y, p.z);
    delay(150);
  }
}
```

**Expected:** raw coordinates change as you touch different positions. **Touch calibration is device- and rotation-dependent.** The paint demo's initial raw coordinate limits are estimates, not a completed calibration. If strokes or buttons don't align with your finger, calibrate X/Y minimums, maximums, axis swap, and inversion before treating the demo as finished.

## Coding Club examples

Where things live in this repository:

```text
coding-club-cyd/
├── README.md
├── docs/
│   └── CYD-28-GUITION-SETUP.md     this guide
└── sketches/
    └── CGCCPaint/
        └── cgcc_paint.ino          the paint demo, with its own README
```

The display test and touch test sketches are the short listings earlier in this guide; paste each into a new Arduino sketch to run it. Arduino requires the `.ino` filename to match its containing sketch folder. The TFT_eSPI setup file must be installed into the library as described above; a copy stored in GitHub does not configure anything by itself.

### Suggested learning activities

1. **Display test:** SPI graphics, RGB colors, screen coordinates, display rotation.
2. **Touch test:** Separate SPI bus, raw sensor readings, coordinate mapping and calibration.
3. **CGCC Paint** ([sketches/CGCCPaint](../sketches/CGCCPaint/)): Event handling, drawing algorithms, UI buttons, brush size and colors.
4. **Wi-Fi scanner (future):** SSIDs, RSSI and signal-strength visualization.
5. **Dashboard (future):** Uptime, free memory, network state and simple sensor readings.

## Troubleshooting

| Symptom | What to check |
|---|---|
| `Unable to verify flash chip connection` | Lower **Upload Speed** from 921600 to **115200**; retry with a good USB data cable. |
| No serial output | Confirm correct port, 115200 Serial Monitor, successful upload, and press RST. |
| Garbled serial text | Confirm 115200 Serial Monitor; reset the board. |
| White screen, sketch runs | Confirm the **ILI9341_2** driver and active TFT_eSPI setup; check backlight GPIO 21. |
| Diagnostic says 320 × 480 | Another display's TFT_eSPI setup is still selected. |
| Serial prints RED/GREEN/BLUE but screen stays white | The sketch runs, but the LCD driver/pin configuration is wrong or the LCD is not responding. |
| Touch doesn't respond | Install XPT2046_Touchscreen and use the separate touch SPI pins listed above. |
| Touch location is wrong | Calibrate raw coordinates and screen orientation; don't assume default min/max values. |
| Sketch compiles but behaves unexpectedly | Check the exact CYD hardware revision and confirm the active library setup. |

## Additional references

- [Amazon product listing — ASIN B0D8W9DSYZ](https://www.amazon.com/dp/B0D8W9DSYZ)
- [TFT_eSPI library](https://github.com/Bodmer/TFT_eSPI)
- [XPT2046_Touchscreen library](https://github.com/PaulStoffregen/XPT2046_Touchscreen)
- [ESP32 Cheap Yellow Display community project](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [2-USB ILI9341 CYD pinout reference](https://lcdwizard.com/boards/cyd-2432s028-2usb-ili9341)

---

**CGCC Coding Club:** Tested settings in this guide apply to our Guition-marked ESP32-2432S028 board. Display and touch tests passed. MicroSD, audio, RGB LED, and other expansion features remain to be tested.
