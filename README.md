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

| Sketch | What it does |
|---|---|
| [generative_landscape](sketches/generative_landscape/) | Procedurally generates a different mountain landscape every time you press the BOOT button — six color themes, layered ridges, lakes, trees, stars, and shooting stars. |

More sketches will be added as the club works through projects.

---

## Repository Layout

```
coding-club-cyd/
├── docs/
│   └── CYD-SETUP.md        Full board setup and troubleshooting guide
├── sketches/
│   └── generative_landscape/
│       └── generative_landscape.ino
├── src/                    Shared code (currently empty)
├── LICENSE
└── README.md
```

Each sketch lives in its own folder with a matching name. Arduino IDE requires this: a sketch named `foo.ino` must sit inside a folder named `foo`. Open a sketch by double-clicking the `.ino` file, or with **File → Open** in Arduino IDE.

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

The red LED only means the board has power. It does **not** mean the cable carries data.

Full details for each of these are in [docs/CYD-SETUP.md](docs/CYD-SETUP.md).

---

## Contributing

Club members: add new sketches under `sketches/<sketch_name>/<sketch_name>.ino`, and include a short `README.md` in the folder explaining what the sketch does, which pins or hardware it uses, and how to interact with it.

---

## License

MIT — see [LICENSE](LICENSE). Copyright © 2026 CGC Computing Studies.
