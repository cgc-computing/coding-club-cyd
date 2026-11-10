# Flip Timer

A one-button stopwatch with a flip-clock display. Four cards show minutes and
seconds (`MM:SS`), and each card flips over when its digit changes.

There is exactly one button. It cycles:

**Start → Stop → Reset → Start …**

The button on screen always shows what the *next* press will do.

---

## Running It

1. Complete [the CYD setup guide](../../docs/CYD-SETUP.md) first.
2. Install **XPT2046_Touchscreen** (Paul Stoffregen) from Library Manager.
3. Open `FlipTimer.ino` in Arduino IDE. `FlipDigits.h` opens as a second tab.
4. Board: **ESP32 Dev Module**, Upload Speed: **115200**, Port: your
   `/dev/cu.usbserial-*` device.
5. Upload.

---

## Using It

| Press | What happens | Button then shows |
|---|---|---|
| 1st | Timer starts counting up | **Stop** (red) |
| 2nd | Timer freezes | **Reset** (amber) |
| 3rd | Cards flip back to `00:00` | **Start** (green) |

A "press" is a tap **anywhere** on the touchscreen, or the **BOOT** button on
the back of the board. The whole screen is the button, so the sketch never
needs to know *where* you touched — which means **no touch calibration**.

The timer stops by itself at `99:59`.

---

## How It Works

**Keeping time.** The sketch does not count ticks. Every pass through `loop()`
it asks "how long since Start was pressed?" using `millis()`. Drawing a flip
takes a few milliseconds, but that can never make the stopwatch run slow.

**The digits.** TFT_eSPI's big number fonts are not turned on in every setup
the club uses, so this sketch draws its own. `FlipDigits.h` describes each
digit as two to four thick strokes — lines and arcs. At startup, each digit is
turned into a small grayscale picture by measuring how far every pixel is from
the nearest stroke. Try changing `STROKE_W` to make the digits bolder or
thinner, or edit a shape in `DIGIT_SHAPES`.

**The flip.** A card is two halves with a hinge between them. A flip has two
parts, about 150 ms each:

1. The old top half tips forward. We draw it squashed toward the hinge and a
   little darker, which uncovers the new digit's top half behind it.
2. The flap carries on down past the hinge, now showing the new digit's bottom
   half, growing until it covers the old one.

Only half a card is redrawn per frame, so even when all four cards flip at once
(`09:59 → 10:00`) the animation stays smooth.

**Debouncing.** Buttons and touch panels flicker for a few milliseconds as
contact is made. `buttonPressed()` only believes a change once the input has
held steady for 40 ms, and reports a press once — when the finger goes down.

---

## Hardware

| Function | GPIO |
|---|---:|
| TFT backlight | 21 (active HIGH) |
| BOOT button | 0 (LOW when pressed) |
| T_CLK | 25 |
| T_CS | 33 |
| T_DIN (MOSI) | 32 |
| T_DO (MISO) | 39 |
| T_IRQ | 36 |

The touchscreen is on its own SPI bus, separate from the display. The display
pins live in TFT_eSPI's setup file, not in this sketch. The only TFT_eSPI font
used is Font 4, for the button label.

---

## Ideas To Try

- Change `FLIP_MS` to make the flips snappier or slower.
- Change the card and digit colors near the top of the sketch.
- Add a third pair of cards for hours.
- Turn it into a countdown timer that flashes the screen at zero.
