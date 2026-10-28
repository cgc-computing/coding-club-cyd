# Coding Club CYD

Setup guides and Arduino sketches for the **ESP32 "CYD" (Cheap Yellow Display)** boards used by the CGC Computing Studies Coding Club.

The CYD is an inexpensive all-in-one board: an ESP32 with Wi-Fi and Bluetooth, a 2.8-inch color touchscreen, and a microSD slot — everything you need for a graphics or sensor project in a single $10–15 package.

---

## The Board

| Item | Value |
|---|---|
| Board | ESP32-2432S028 (dual-USB version) |
| MCU | ESP32-D0WD-V3 rev 3.1 |
| Display | 2.8-inch 240×320 ILI9341 TFT |
| Landscape resolution | 320×240 |
| Touch | Resistive |
| Extras | microSD slot, Wi-Fi, Bluetooth |
| Backlight | GPIO 21 |

---

## Start Here

**New to the board? Read [docs/CYD-SETUP.md](docs/CYD-SETUP.md) first.**

The other two guides in `docs/` cover different hardware:

- [docs/CYD-28-GUITION-SETUP.md](docs/CYD-28-GUITION-SETUP.md) — the Guition / DIYmalls 2.8-inch variant, including a touch test and the pin assignments used by the touch sketches.
- [docs/HOSYOND-4IN-SETUP.md](docs/HOSYOND-4IN-SETUP.md) — the separate Hosyond 4-inch 480×320 ST7796S board. Its display settings are **not** interchangeable with the 2.8-inch CYD.

That guide takes you from a brand-new computer to a verified working display:

1. Install Arduino IDE 2.x (and Rosetta, on Apple Silicon Macs)
2. Add ESP32 board support
3. Pick the right board settings — **ESP32 Dev Module** at **115200** baud
4. Install and configure **TFT_eSPI** for our exact wiring
5. Upload a verification sketch and confirm the whole screen works

Two settings in particular trip people up on these boards:

- The display driver must be **`ILI9341_2`**, not `ILI9341`. With the wrong one, only part of the screen updates.
- The upload speed must be **115200**. The default 921600 connects but often fails mid-write.

Don't skip the setup guide — every sketch in this repo assumes TFT_eSPI is already configured correctly.

---

## Sketches

| Sketch | What it does | Extra hardware used |
|---|---|---|
| [generative_landscape](sketches/generative_landscape/) | Procedurally generates a different mountain landscape every time you press the BOOT button — six color themes, layered ridges, lakes, trees, stars, and shooting stars. | BOOT button |
| [CYDSolarSystem](sketches/CYDSolarSystem/) | An animated orrery. The Sun and all eight planets orbit in the right order at compressed speeds, with Saturn's rings, Jupiter's cloud bands, and a side panel that tours each planet's real diameter, distance, and year. | none |
| [WiFiDetectiveDeluxe](sketches/WiFiDetectiveDeluxe/) | A handheld Wi-Fi analyzer: discovers nearby access points, charts channel congestion, and tracks signal strength over time. **Phase 1 of 6 — menu and touch navigation only so far.** | touchscreen, Wi-Fi |
| [CGCCPaint](sketches/CGCCPaint/) | A small touchscreen paint program: six colors, adjustable brush, clear button. Runs in portrait. Touch calibration values are starting estimates — see its README. | touchscreen |

More sketches will be added as the club works through projects.

### Build sizes

These compile against ESP32 core 3.3.12 and TFT_eSPI 2.5.43 (CGCCPaint has not been measured yet):

| Sketch | Flash | RAM |
|---|---:|---:|
| generative_landscape | 322 KB (24%) | 23 KB (7%) |
| CYDSolarSystem | 338 KB (25%) | 23 KB (7%) |
| WiFiDetectiveDeluxe | 945 KB (72%) | 47 KB (14%) |

Wi-Fi Detective is much larger because linking the Wi-Fi stack pulls in a
lot of code. That is normal and still leaves plenty of room.

---

## Repository Layout

```
coding-club-cyd/
├── docs/
│   ├── CYD-SETUP.md               Full 2.8-inch CYD setup and troubleshooting guide
│   ├── CYD-28-GUITION-SETUP.md    Guition / DIYmalls 2.8-inch variant, with touch test
│   └── HOSYOND-4IN-SETUP.md       Hosyond 4-inch ST7796S board (different hardware)
├── sketches/
│   ├── generative_landscape/
│   │   └── generative_landscape.ino
│   ├── CYDSolarSystem/
│   │   └── CYDSolarSystem.ino
│   ├── WiFiDetectiveDeluxe/       Multi-file sketch — see its own README
│   │   ├── WiFiDetectiveDeluxe.ino
│   │   ├── Config.h
│   │   ├── DisplayManager.{h,cpp}
│   │   ├── TouchManager.{h,cpp}
│   │   └── UIComponents.{h,cpp}
│   └── CGCCPaint/
│       └── cgcc_paint.ino
├── LICENSE
└── README.md
```

Each sketch lives in its own folder under `sketches/`. Arduino IDE requires the folder name to match the sketch name: a sketch named `foo.ino` must sit inside a folder named `foo`. Open a sketch by double-clicking the `.ino` file, or with **File → Open** in Arduino IDE.

A sketch can be more than one file. `WiFiDetectiveDeluxe` is split across several `.h`/`.cpp` files in its folder, and Arduino IDE opens all of them as tabs automatically. That is the normal way to keep a larger project readable.

---

## Quick Start for a New Sketch

1. Finish [docs/CYD-SETUP.md](docs/CYD-SETUP.md) once, on your machine.
2. Open the `.ino` file from the sketch folder you want.
3. **Tools → Board →** ESP32 Dev Module
4. **Tools → Upload Speed →** 115200
5. **Tools → Port →** your `/dev/cu.usbserial-*` device
6. Click **Upload**.

If the port disappears or the number changes after you unplug the board, that's normal — just reselect it.

---

## Troubleshooting Cheat Sheet

| Symptom | Likely fix |
|---|---|
| Screen is all white | TFT_eSPI config is wrong — check pins, driver, setup file |
| Only part of the screen updates | Use the `ILI9341_2` driver, not `ILI9341` |
| Colors look inverted | Sketch needs `tft.invertDisplay(true)` |
| Image is sideways | Use `tft.setRotation(1)` for 320×240 landscape |
| Upload fails partway | Drop upload speed to 115200 |
| No serial port appears | Try a different **data** cable; USB-A → USB-C through a dock worked best for us |
| Touch does nothing | The touchscreen is on its own SPI bus, separate from the display — see [WiFiDetectiveDeluxe](sketches/WiFiDetectiveDeluxe/) |
| Taps land in the wrong place | The touchscreen needs calibrating, not rewiring. `WiFiDetectiveDeluxe` has an on-device calibration screen; `CGCCPaint` uses fixed constants you adjust by hand |
| `ledcAttach was not declared` | ESP32 core 2.x is installed; some sketches need core 3.x |

The red LED only means the board has power. It does **not** mean the cable carries data.

Full details for each of these are in [docs/CYD-SETUP.md](docs/CYD-SETUP.md).

---

## Contributing

Club members: add new sketches under `sketches/<sketch_name>/<sketch_name>.ino`, and include a short `README.md` in the folder explaining what the sketch does, which pins or hardware it uses, and how to interact with it.

If your sketch needs a library beyond TFT_eSPI, say so in its README with the exact version you tested, so the next person can reproduce your build.

---

## License

MIT — see [LICENSE](LICENSE). Copyright © 2026 CGC Computing Studies.
