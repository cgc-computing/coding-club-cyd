# Generative Landscape

A procedural landscape generator for the ESP32 CYD. Every press of the BOOT button paints a brand-new mountain scene — different sky, different peaks, different weather — and no two are the same.

Nothing is stored as an image. The whole picture is drawn from a single random number.

---

## Running It

1. Complete [the CYD setup guide](../../docs/CYD-SETUP.md) first. This sketch needs TFT_eSPI configured for our boards (driver `ILI9341_2`, backlight on GPIO 21).
2. Open `generative_landscape.ino` in Arduino IDE.
3. Board: **ESP32 Dev Module**, Upload Speed: **115200**, Port: your `/dev/cu.usbserial-*` device.
4. Upload.

A landscape appears as soon as the board boots.

### Controls

The only control is the **BOOT button** on the back of the board (GPIO 0).

| Action | Result |
|---|---|
| **Short press** (tap) | Generate a brand-new world with a new random seed |
| **Long press** (hold ~0.7 s or more) | Redraw the *same* world from the current seed |

The long press is a good demo: because the drawing is driven entirely by the seed, redrawing reproduces the scene pixel for pixel. The seed is printed in the footer as `WORLD #1234567890`.

---

## What Gets Generated

Every world is assembled in stages, each with its own randomness.

**Theme** — one of six palettes, picked at random:

| | Theme | Time of day |
|---|---|---|
| 0 | Blue alpine | Day |
| 1 | Sunset | Day |
| 2 | Pink fantasy | Day |
| 3 | Alien teal | Day |
| 4 | Blue night | Night |
| 5 | Purple night | Night |

The theme sets every color in the scene — sky, mountains, snow, water, trees — and a `night` flag that changes what else can appear.

**Sky** — a vertical gradient blended line by line from `skyTop` to `skyBottom`.

**Atmosphere** — depends on the theme:

- Night worlds get 25–55 stars, and an 18% chance of a shooting star with a fading tail.
- Day worlds have a 75% chance of clouds (1–4 of them, built from overlapping circles).
- Day worlds also have a 20% chance of a small flock of birds.

**Sun or moon** — a circle of radius 15–23 placed in the upper part of the sky. At night there's a 65% chance it becomes a crescent, carved out by drawing a second circle in the sky color.

**Mountains** — four ridges, drawn back to front so nearer ones overlap farther ones:

| Ridge | Base Y | Peak range |
|---|---:|---|
| Far | 135 | 72–108 |
| Mid | 160 | 95–125 |
| Near | 188 | 118–150 |
| Foreground (ground) | 214 | 165–195 |

Each ridge is a zig-zag of peaks and valleys generated left to right, then filled by drawing a vertical line from the ridge down to the bottom of the screen. The far ridge gets snow caps on roughly 70% of its peaks; the mid ridge has a 50% chance of any snow at all.

**Lake** — a 42% chance. When present, the bottom of the scene becomes water with horizontal ripple texture and a widening reflection of the sun or moon directly below it.

**Trees** — 6–12 pine trees scattered along the foreground ridge, each placed at the exact terrain height for its x position. Two larger framing trees at the left and right edges each have a 65% chance of appearing.

**Footer** — a black bar showing the world seed and the button hint.

---

## How It Works

The core idea is **deterministic generation from a seed**:

```cpp
worldSeed = esp_random();   // a true random number from ESP32 hardware
randomSeed(worldSeed);      // seed the pseudo-random generator
// ...every random() call after this is determined by worldSeed
```

Everything that follows — which theme, how many stars, where each peak lands — comes from `random()`. Same seed in, same picture out. That's why the long-press redraw reproduces the scene exactly, and why a single 32-bit number is enough to describe an entire landscape.

### Notable techniques in the code

**Color blending** (`blend565`) — the display uses 16-bit RGB565 color. The helper functions pull the red, green, and blue channels back out, interpolate between two colors, and repack them. This is what makes the sky gradient and the shooting star's fading tail possible.

**Ridge as data, not pixels** (`struct Ridge`) — each ridge is stored as arrays of x and y points, up to `MAX_POINTS`. Storing the shape separately from the drawing means you can ask questions about it later, which is exactly what tree placement needs.

**Terrain lookup** (`ridgeYAt`) — given an x coordinate, this walks the ridge segments, finds the one containing x, and interpolates the height. Trees use it so they sit *on* the ground instead of floating above or sinking below it.

**Painter's algorithm** — the scene is drawn strictly back to front: sky, then atmosphere, then far → near mountains, then ground, then trees, then UI. There's no depth buffer; later drawing simply covers earlier drawing. (Watch for the one place the near ridge is drawn twice — after the lake is painted, its edge has to be restored.)

**Button debouncing** (`updateButton`) — a physical button bounces, making one press look like several. The code ignores state changes for 40 ms after a transition, then measures how long the button was held to tell a tap from a hold.

---

## Things to Try

Good starting points if you want to modify it:

- **Add a seventh theme.** Copy a `case` block in `chooseTheme()`, change the colors, and bump the `random(6)` to `random(7)`.
- **Change the odds.** Make lakes rare or guaranteed by editing `chance(42)`. Make every night sky get a shooting star with `chance(100)`.
- **More mountains.** Add a fifth ridge between two existing ones, with its own base Y and color.
- **Different trees.** Write a `drawBirchTree()` or `drawCactus()` and pick between them at random in `drawTrees()`.
- **Animate something.** Make the clouds drift, or stars twinkle, by redrawing small regions in `loop()`.
- **Fix the seed.** Replace `esp_random()` with a constant to always generate the same world — useful while you're working on one specific part.
- **Print the seed over serial** so you can note down worlds you like and reproduce them later.

---

## Hardware and Pins

| Function | Pin |
|---|---|
| TFT backlight | GPIO 21 (driven HIGH) |
| BOOT button | GPIO 0 (`INPUT_PULLUP`, active LOW) |
| Display | via TFT_eSPI — see [setup guide](../../docs/CYD-SETUP.md) |

Screen is used in rotation 1 (landscape, 320×240) with `invertDisplay(true)`.

### Libraries

- **TFT_eSPI** by Bodmer (tested with 2.5.43)
- `esp_system.h` — part of the ESP32 core, provides `esp_random()`

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Blank or white screen | TFT_eSPI isn't configured — go back to the [setup guide](../../docs/CYD-SETUP.md) |
| Only the left portion draws | Wrong driver; must be `ILI9341_2` |
| Colors look wrong/negative | `invertDisplay(true)` missing or doubled up |
| Button does nothing | Make sure you're pressing **BOOT**, not **RST**. RST restarts the board (which also makes a new world, just slower) |
| One press makes several worlds | Button bounce — increase `DEBOUNCE_MS` |
