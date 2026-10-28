# Wi-Fi Detective Deluxe

A handheld Wi-Fi discovery and analysis instrument for the ESP32 CYD.

Point it at a room and it shows you the wireless environment around you: which
access points are nearby, how strong they are, which channels are crowded, and
how one network's signal changes as you walk.

> **Status: Phase 1 of 6 complete.**
> Hardware bring-up, touch calibration, the main menu and screen navigation are
> built and compile cleanly. The six feature screens are reachable and say which
> phase will implement them. Nothing in this sketch invents network data.
>
> **Not yet tested on hardware.** The code has been compiled, not uploaded.

---

## Scope

This is a **passive** tool. It listens for the beacon frames access points
already broadcast, the same way your phone does when it shows a Wi-Fi list.

It does not — and will not — implement deauthentication, disassociation,
packet injection, evil-twin access points, credential capture, jamming, or any
attempt to join a network it has not been given credentials for.

---

## Hardware

| Item | Value |
|---|---|
| Board | ESP32-2432S028R ("Cheap Yellow Display", dual-USB revision) |
| MCU | ESP32-D0WD-V3, dual core, 240 MHz |
| Display | 2.8-inch 240×320 ILI9341, used at 320×240 landscape |
| Touch | XPT2046 resistive, on its own SPI bus |
| Flash | 4 MB |
| Radio | 2.4 GHz 802.11 b/g/n |

### Pin configuration

**Display** — these go in TFT_eSPI's `User_Setup.h`, not in this sketch:

| Function | GPIO |
|---|---:|
| TFT MISO | 12 |
| TFT MOSI | 13 |
| TFT SCLK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT RST | −1 (not wired) |
| TFT backlight | 21 (active HIGH) |

**Touch** — these live in `Config.h`:

| Function | GPIO |
|---|---:|
| T_CLK | 25 |
| T_CS | 33 |
| T_DIN (MOSI) | 32 |
| T_OUT (MISO) | 39 |
| T_IRQ | 36 |

The touch panel is **not** on the display's SPI bus. The display runs on HSPI;
the sketch opens a second SPI bus (VSPI) on the pins above for the XPT2046.
This is why TFT_eSPI's own touch support is deliberately left disabled.

---

## Dependencies

| Dependency | Version used |
|---|---|
| ESP32 Arduino core | **3.x** (tested against 3.3.12) |
| TFT_eSPI (Bodmer) | 2.5.43 |
| XPT2046_Touchscreen (PaulStoffregen) | 1.4.0 |
| `Preferences`, `WiFi`, `SPI` | bundled with the ESP32 core |

**Core 3.x is required**, not optional. `DisplayManager` uses `ledcAttach()` for
backlight PWM, which does not exist on core 2.x.

---

## Build instructions

Complete [the club's CYD setup guide](../../docs/CYD-SETUP.md) first. This sketch
assumes TFT_eSPI is already configured for our boards.

### 1. TFT_eSPI configuration

Edit `~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h`:

```c
#define ILI9341_2_DRIVER      // NOT ILI9341 -- see note below
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  20000000
#define USE_HSPI_PORT
```

Fonts 2, 4 and 6 are all used by the UI. If you drop any of them the sketch
compiles but text silently fails to render.

Check that `User_Setup_Select.h` still includes `User_Setup.h` and not one of
the bundled board presets.

### 2. Install the touch library

**Sketch → Include Library → Manage Libraries →** search `XPT2046_Touchscreen`
by Paul Stoffregen. Install 1.4.0.

### 3. Board settings

| Setting | Value |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | **115200** |
| CPU Frequency | 240 MHz |
| Flash Mode | DIO |
| Flash Size | 4 MB |
| Partition Scheme | Default 4 MB with SPIFFS |
| PSRAM | Disabled |

921600 baud connects but frequently fails mid-write on our boards.

### 4. Upload

Open `WiFiDetectiveDeluxe.ino`, pick your `/dev/cu.usbserial-*` port, click
Upload. Serial Monitor at 115200 baud reports the calibration values and free
heap at boot.

### Command-line build

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PSRAM=disabled,FlashFreq=80,UploadSpeed=115200 WiFiDetectiveDeluxe
```

Current Phase 1 build: **945 KB flash (72%)**, **47 KB RAM (14%)**.

### Expected compiler warning

```
#warning >>>>------>> TOUCH_CS pin not defined, TFT_eSPI touch functions will not be available!
```

This is correct and expected. TFT_eSPI's touch support assumes the touch
controller shares the display's SPI bus, which is false on the CYD. The sketch
drives the XPT2046 itself on a separate bus.

---

## Touch calibration

The first time the sketch runs on a board it finds no calibration in NVS and
goes straight into the calibration screen. The fallback numbers in `Config.h`
are generic XPT2046 ranges, not measurements from your panel, so the sketch
does not quietly trust them.

**The procedure:** four targets appear, one at a time, inset from each corner.
Tap the centre of each as precisely as you can. A target turns green when its
reading is accepted. Calibration is saved to NVS and survives reflashing the
sketch.

From the measurements the sketch works out, on its own:

- whether the panel's X axis maps to the screen's X or Y axis,
- whether either axis is inverted,
- the raw ADC value at each screen edge, extrapolated outward from the targets.

A result where the four taps were too close together is rejected and the
procedure repeats, so a mis-tap cannot leave the device unusable.

**To recalibrate later:** Main menu → SETTINGS → RECALIBRATE.

Calibration is stored in the NVS namespace `wifidet` under keys `calOK`,
`calSwap`, `calXMin`, `calXMax`, `calYMin`, `calYMax`.

---

## Features

### Working now (Phase 1)

- Display bring-up with PWM backlight control
- XPT2046 touch on its own SPI bus, in polling mode
- Debounced press / release / tap / drag detection
- Four-corner touch calibration with automatic axis detection, saved to NVS
- Animated splash screen (tap to skip)
- Six-item touchscreen main menu with pressed-state feedback
- Screen navigation state machine with working BACK on every screen
- Live free-heap readout in the status bar
- Reusable UI primitives: header, status bar, panel, button, signal meter

### Planned

| Phase | Screen | What it will do |
|---|---|---|
| 2 | NETWORKS / DETAILS | Non-blocking scan, scrollable sorted AP list, full per-AP record |
| 3 | CHANNELS | 2.4 GHz occupancy bar chart, overlap graph, congestion estimate |
| 4 | RADAR | Animated sweep, APs as blips at stable BSSID-derived angles |
| 5 | LIVE MONITOR | RSSI history graph for one BSSID, min/max/average, missed-scan count |
| 6 | SETTINGS | Brightness, scan interval, theme, radar speed, debug toggle |

---

## Known limitations

**Not yet hardware-tested.** Phase 1 compiles cleanly and the logic has been
reviewed, but it has not been uploaded to a board. The touch pin assignments in
particular are the documented ESP32-2432S028R mapping rather than values
measured on your unit.

**Touch runs in polling mode.** `TOUCH_USE_IRQ` in `Config.h` defaults to
`false`. The XPT2046 library can use T_IRQ to skip SPI reads, but if T_IRQ is
not wired as expected on a given board revision, touch silently stops working
altogether. Polling costs a few SPI transfers per loop and always works.

**Calibration is linear, not affine.** It corrects axis swap, inversion and
scale, but not rotational skew between the touch panel and the glass. On these
panels that is not measurable by hand.

**The splash progress bar is drawn, not measured.** It tracks elapsed time
against a fixed duration. It is not reporting work being done.

### Things that will matter in later phases

- **RSSI is not distance.** It depends on obstacles, antenna orientation,
  transmit power and interference. The radar screen will use stable
  pseudo-random angles derived from each BSSID and will be labelled
  *visualisation only*. It cannot and will not locate anything physically.
- **An SSID is not an access point.** Several APs commonly advertise the same
  SSID. BSSID is the identifier that means one radio.
- **Congestion will be an estimate.** It will be computed from AP count, RSSI
  and channel overlap. Actual airtime utilisation requires capturing traffic,
  which this tool does not do.
- **2.4 GHz only.** The ESP32's radio cannot see 5 GHz or 6 GHz networks, so
  any AP on those bands is invisible here.
- **Which channels appear depends on region.** Channels 12–14 are not used in
  the US. The analyser will distinguish channels actually observed from the
  full 1–14 range.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Blank or white screen | TFT_eSPI not configured — see the setup guide |
| Only part of the screen updates | Wrong driver; must be `ILI9341_2`, not `ILI9341` |
| Colours look like a photo negative | `TFT_INVERT` in `Config.h` got turned off |
| Touch does nothing at all | Check the five touch pins in `Config.h`; confirm `TOUCH_USE_IRQ` is `false` |
| Taps land offset from where you touched | Recalibrate: SETTINGS → RECALIBRATE |
| Calibration screen repeats forever | Taps are landing too close together — tap the centre of each target |
| Calibration screen every boot | NVS write is failing; check the partition scheme includes NVS |
| `ledcAttach was not declared` | ESP32 core 2.x installed; this sketch needs 3.x |
| Text missing but layout correct | A `LOAD_FONT*` line is missing from `User_Setup.h` |
| Upload fails partway | Drop upload speed to 115200 |

---

## Code layout

```
WiFiDetectiveDeluxe/
├── WiFiDetectiveDeluxe.ino   Screen state machine, menu, setup/loop
├── Config.h                  Pins, geometry, palette, tuning constants
├── DisplayManager.{h,cpp}    TFT_eSPI ownership, backlight PWM
├── TouchManager.{h,cpp}      XPT2046 sampling, debounce, calibration
├── UIComponents.{h,cpp}      Header, footer, panel, button, signal meter
└── README.md
```

Wi-Fi scanning (Phase 2) will go in its own `WiFiScanner` module and will not
draw anything, so scan timing and rendering stay independent of each other.

---

## For club members: things to try

- Change the colour palette in `Config.h` — every screen picks it up.
- Add a seventh menu entry: one line in `MENU_ENTRIES`, bump `MENU_COUNT`, and
  the layout maths handles the rest.
- Read `TouchManager::update()` and work out why acting on *release* rather
  than *press* makes the buttons feel better.
- Print raw touch values over serial and watch what the calibration is
  actually correcting for.

---

MIT — see [LICENSE](../../LICENSE). Copyright © 2026 CGC Computing Studies.
