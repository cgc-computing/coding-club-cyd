# CGCC Paint

A small touchscreen paint program for the ESP32 CYD. Drag a finger across the
canvas to draw, tap a color to switch, tap **BRUSH** to cycle the brush size,
tap **CLEAR** to wipe the canvas.

This is the simplest touch sketch in the repo and a good first project for
seeing how the display and the touchscreen fit together.

---

## Running It

1. Complete [the CYD setup guide](../../docs/CYD-SETUP.md) first. The
   [Guition 2.8-inch guide](../../docs/CYD-28-GUITION-SETUP.md) has a raw touch
   test that is worth running before this sketch.
2. Install **XPT2046_Touchscreen** (Paul Stoffregen) from Library Manager. The
   other sketches only need TFT_eSPI; this one also needs the touch library.
3. Open `cgcc_paint.ino` in Arduino IDE.
4. Board: **ESP32 Dev Module**, Upload Speed: **115200**, Port: your
   `/dev/cu.usbserial-*` device.
5. Upload.

Note that this sketch uses **portrait** orientation (`setRotation(0)`,
240 wide × 320 tall), unlike the other sketches in the repo, which run in
320×240 landscape.

---

## What's On Screen

| Region | Y range | Contents |
|---|---|---|
| Header | 0–31 | `CGCC PAINT` title |
| Canvas | 36–245 | Drawing area |
| Palette | 250–287 | Six color swatches; the selected one has a white ring |
| Buttons | 291–315 | **BRUSH: n** on the left, **CLEAR** on the right |

Colors: white, red, yellow, green, blue, magenta. Brush size cycles
2 → 4 → 6 → 8 → 10 → 2.

Strokes are drawn as a line of filled circles between the previous and current
touch point, so fast movements do not leave gaps.

---

## Hardware

| Function | GPIO |
|---|---:|
| TFT backlight | 21 (active HIGH) |
| T_CLK | 25 |
| T_CS | 33 |
| T_DIN (MOSI) | 32 |
| T_DO (MISO) | 39 |
| T_IRQ | 36 |

The touchscreen is on its own SPI bus (VSPI), separate from the display. The
display pins live in TFT_eSPI's `User_Setup.h`, not in this sketch.

---

## Touch Calibration

The raw-to-pixel mapping is set by constants at the top of the sketch:

```cpp
#define RAW_X_MIN 200
#define RAW_X_MAX 3900
#define RAW_Y_MIN 200
#define RAW_Y_MAX 3900

#define SWAP_XY  false
#define INVERT_X false
#define INVERT_Y false
```

**These are starting estimates, not a measured calibration.** Resistive panels
vary from unit to unit. If strokes land away from your finger, or the palette
and buttons do not respond where you tap:

1. Open Serial Monitor at 115200. The sketch prints the mapped `X=.. Y=..` for
   every touch.
2. Touch each corner of the screen and note the printed values.
3. If left/right or top/bottom are reversed, flip `INVERT_X` or `INVERT_Y`.
   If moving your finger sideways changes Y instead of X, set `SWAP_XY` to
   `true`.
4. Adjust the `RAW_*` limits until the corners map close to 0 and 239 / 319.

For a proper on-device calibration flow that stores its result in flash, see
[WiFiDetectiveDeluxe](../WiFiDetectiveDeluxe/), whose `TouchManager` does
exactly that.

---

## Libraries

| Library | Purpose |
|---|---|
| TFT_eSPI | Display |
| XPT2046_Touchscreen | Touch controller |
| SPI | Second SPI bus for touch |
