# CYD Solar System

An animated orrery for the ESP32 CYD. The Sun sits at the centre, all eight
planets sweep around it, and a side panel tours them one at a time showing how
big each one is, how far out it orbits, and how long its year lasts.

Nothing to press, nothing to connect. Power the board and it runs.

---

## Running It

1. Complete [the CYD setup guide](../../docs/CYD-SETUP.md) first. This sketch needs
   TFT_eSPI configured for our boards (driver `ILI9341_2`, backlight on GPIO 21).
2. Open `CYDSolarSystem.ino` in Arduino IDE.
3. Board: **ESP32 Dev Module**, Upload Speed: **115200**, Port: your
   `/dev/cu.usbserial-*` device.
4. Upload.

The animation starts as soon as the board boots.

### Controls

There are none. No touchscreen, no buttons, no Wi-Fi. The info panel advances
to the next planet every 5 seconds on its own, starting from Earth.

---

## What's On Screen

The display is split into three fixed regions:

| Region | Area | Contents |
|---|---|---|
| Header | full width, top 25 px | Title bar |
| Orbit view | 220 × 190, below the header | Starfield, Sun, orbit paths, planets |
| Info panel | 95 px wide, right edge | The currently selected planet |
| Footer | full width, bottom 25 px | `AUTO TOUR` and `8 PLANETS` |

Only the orbit view is redrawn. The header, footer, and info panel are painted
once and then left alone until the selection changes.

### In the orbit view

- **65 stars**, scattered once at startup and never moved.
- **Faint orbit rings**, one per planet.
- **The Sun**, at the centre of the orbit area.
- **Eight planets**, each a filled circle with a small white highlight.
- **A cyan ring** around whichever planet the info panel is currently showing.

Three planets get extra detail:

| Planet | Detail |
|---|---|
| Earth | A small green landmass |
| Jupiter | Two horizontal cloud bands |
| Saturn | Two ellipses behind the planet and an arc in front, so the rings pass behind and before the body |

### In the info panel

The selected planet's name, a large preview circle, and three real figures:
diameter in km, distance from the Sun in AU, and orbital period in years.
Saturn's preview gets an ellipse drawn around it.

---

## The Planet Data

These values are real.

| Planet | Diameter (km) | Distance (AU) | Orbital period (yr) |
|---|---:|---:|---:|
| Mercury | 4,879 | 0.39 | 0.241 |
| Venus | 12,104 | 0.72 | 0.615 |
| Earth | 12,742 | 1.00 | 1.000 |
| Mars | 6,779 | 1.52 | 1.881 |
| Jupiter | 139,820 | 5.20 | 11.862 |
| Saturn | 116,460 | 9.58 | 29.457 |
| Uranus | 50,724 | 19.20 | 84.017 |
| Neptune | 49,244 | 30.05 | 164.790 |

**What is not to scale:** the orbit radii on screen, the drawn planet sizes, and
the animation speed. Jupiter's real diameter is about eleven times Earth's; on
screen it is less than twice. Neptune really orbits 30 times further out than
Earth; on screen it is about twice as far. Everything has been squashed to fit
220 pixels.

---

## How It Works

### Why the planets don't move at their real speeds

If the animation used the real periods directly, Neptune would take 165 times
as long as Earth to go round. By the time Neptune finished one lap, Mercury
would have completed about 680 — a blur in the middle and a nearly motionless
dot at the edge.

So the sketch takes the **square root** of each period:

```cpp
float visualPeriod = sqrtf(p.period);
```

That keeps the ordering honest — the outer planets are still slower, every
time — while pulling the spread from 680:1 down to about 26:1. At the default
speed of 0.12 simulated years per real second:

| Planet | One lap on screen |
|---|---:|
| Mercury | ~4 s |
| Earth | ~8 s |
| Jupiter | ~29 s |
| Saturn | ~45 s |
| Neptune | ~107 s |

The code comment says it plainly: *"This is intentionally not a physical
simulation."* The numbers in the info panel are real; the motion is staged so
you can watch it.

### Sprite rendering

Drawing an animation directly to the screen causes flicker: you see the old
frame being erased before the new one appears. This sketch avoids that by
drawing each frame into an off-screen buffer and sending the whole finished
frame at once:

```cpp
spaceSprite.fillSprite(BLACK);
drawStars();
drawOrbitPaths();
drawSun();
for (int i = 0; i < PLANET_COUNT; i++) drawPlanet(i);
spaceSprite.pushSprite(SPACE_X, SPACE_Y);
```

That buffer is 220 × 190 pixels at 2 bytes each — **about 82 KB of RAM**, a
quarter of everything the ESP32 has. This is why the orbit view is 220 px wide
rather than the full 320: a full-screen 320 × 240 sprite would need 150 KB and
would likely fail to allocate.

The sketch checks:

```cpp
void* buffer = spaceSprite.createSprite(SPACE_W, SPACE_H);
if (buffer == nullptr) { /* print SPRITE ALLOCATION FAILED and halt */ }
```

If you see **SPRITE ALLOCATION FAILED** in red, the allocation did not succeed.
Shrink `SPACE_W` or `SPACE_H` rather than hoping.

### Frame timing

The loop does not call `delay()`. It compares `millis()` against the last frame
time and only redraws when 50 ms have passed, giving roughly 20 frames per
second. Position depends on `simulationYears`, which advances by real elapsed
time, so the planets move at the same rate regardless of how long a frame
actually took.

---

## Hardware and Pins

| Function | Pin |
|---|---|
| TFT backlight | GPIO 21 (driven HIGH) |
| Display | via TFT_eSPI — see [setup guide](../../docs/CYD-SETUP.md) |

No touchscreen, no buttons, no Wi-Fi, no SD card.

Screen is used in rotation 1 (landscape, 320 × 240) with `invertDisplay(true)`.

### Libraries

- **TFT_eSPI** by Bodmer (tested with 2.5.43)
- `math.h` — for `sqrtf`, `cosf`, `sinf`

### Build size

338 KB flash (25%), 23 KB static RAM (7%), plus the ~82 KB sprite allocated at
startup.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| `SPRITE ALLOCATION FAILED` in red | Not enough free RAM — reduce `SPACE_W` / `SPACE_H` |
| Blank or white screen | TFT_eSPI isn't configured — go back to the [setup guide](../../docs/CYD-SETUP.md) |
| Only the left portion draws | Wrong driver; must be `ILI9341_2` |
| Colours look negative | `invertDisplay(true)` missing or applied twice |
| Planets flicker | The sprite failed and something is drawing straight to the TFT |
| Animation is jerky | `FRAME_INTERVAL` raised too high, or serial output is slowing the loop |
| Planets barely move | `animationSpeed` is too low — it is in simulated years per real second |

---

## Things to Try

- **Speed it up or slow it down.** `animationSpeed` is simulated years per real
  second. Try `0.5` for a fast tour or `0.02` for a calm one.
- **Use the real periods.** Replace `sqrtf(p.period)` with `p.period` and see
  for yourself why the square root is there.
- **Add a moon.** Give Earth a small circle orbiting it, using the same
  `cosf`/`sinf` maths with Earth's position as the centre instead of the Sun's.
- **Add Pluto**, with a note about why it is not on the list.
- **Make the tour manual.** Read the BOOT button (GPIO 0) and step the selection
  on each press — `generative_landscape` has debouncing code you can borrow.
- **Scale the orbits properly** using `distanceAU` and see how unusable it gets.
  That is the real lesson about the solar system's emptiness.
- **Trace the orbits.** Leave a fading trail behind each planet instead of
  clearing the whole sprite each frame.

---

MIT — see [LICENSE](../../LICENSE). Copyright © 2026 CGC Computing Studies.
