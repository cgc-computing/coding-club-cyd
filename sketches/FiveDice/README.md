# Five Dice

A solo dice game for the touchscreen, from the same family as Yacht, Yatzy,
and Generala. Roll five dice, keep the ones you like, and fill in a scorecard
one row per turn. Your best total is saved between power-ups.

---

## Running It

1. Complete [the CYD setup guide](../../docs/CYD-SETUP.md) first.
2. Install **XPT2046_Touchscreen** (Paul Stoffregen) from Library Manager.
3. Open `FiveDice.ino` in Arduino IDE.
4. Board: **ESP32 Dev Module**, Upload Speed: **115200**, Port: your
   `/dev/cu.usbserial-*` device.
5. Upload.

**First run:** the screen asks you to tap three white crosses. That teaches
the sketch how your touch panel lines up with the display, and the answer is
saved in flash. If taps ever land in the wrong place, press the **BOOT**
button to do it again. A stylus or fingernail is more accurate than a
fingertip.

---

## How To Play

A game is 13 turns. On each turn:

1. Tap **ROLL**. The white dots on the button show how many rolls are left
   (three per turn).
2. Tap any dice you want to keep. They get an orange rim and a **HOLD**
   label, and stay put on the next roll. Tap again to let go.
3. Roll again if you like, up to three rolls in total.
4. Tap a row on the scorecard. It turns yellow. **Tap the same row again** to
   lock it in — the two taps are there so a stray touch cannot spend a row.

The green number on each open row is what your current dice would score
there. Each row can be used once, so sometimes you have to take a zero.

When the card is full the button turns into **NEW GAME**.

### Scoring

| Row | What you need | Points |
|---|---|---|
| Ones … Sixes | Any dice | Sum of the dice showing that number |
| Bonus | 63 or more across Ones–Sixes | 35 |
| 3 of Kind | Three dice the same | Sum of all five dice |
| 4 of Kind | Four dice the same | Sum of all five dice |
| FullHouse | Three of one number and two of another | 25 |
| Small Run | Four in a row, such as 2-3-4-5 | 30 |
| Large Run | Five in a row | 40 |
| 5 of Kind | All five dice the same | 50 |
| Chance | Anything | Sum of all five dice |

---

## How It Works

**Scoring.** `scoreFor()` first counts how many dice show each face. Almost
every row can then be answered from two facts: the size of the biggest group
of matching dice, and the longest run of faces that appear at least once.

**Previews.** After each roll, every open row is redrawn with `scoreFor()` for
the dice on the table — the same function that is used when you lock a row in.

**Touch.** `getTap()` reports a tap once, when the finger goes down and has
stayed down for 40 ms. The raw reading is averaged over eight samples and
mapped to pixels with the saved calibration. Three crosses are enough to work
out whether the panel's axes are swapped or flipped, so there are no
`INVERT_X`-style settings to edit by hand.

**Saving.** The calibration and best score are stored with the ESP32
`Preferences` library, which keeps small values in flash memory.

---

## Hardware

| Function | GPIO |
|---|---:|
| TFT backlight | 21 (active HIGH) |
| BOOT button | 0 (LOW when pressed) — redo touch calibration |
| T_CLK | 25 |
| T_CS | 33 |
| T_DIN (MOSI) | 32 |
| T_DO (MISO) | 39 |
| T_IRQ | 36 |

The touchscreen is on its own SPI bus, separate from the display. The display
pins live in TFT_eSPI's setup file, not in this sketch. Only the built-in
font (Font 1) is used.

---

## Ideas To Try

- Add a second player and take turns on two scorecards.
- Give 100 bonus points for every extra 5 of Kind after the first.
- Make the dice tumble for longer, or beep a buzzer on each roll.
- Change the table and dice colors near the top of the sketch.
- Write a computer opponent that picks the best-scoring row.
