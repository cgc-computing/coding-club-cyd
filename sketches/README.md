# Sketches

Arduino sketches for the ESP32 CYD boards used by the Coding Club. Each one
lives in its own folder and has its own README.

**Before running any of these, finish [the CYD setup guide](../docs/CYD-SETUP.md).**
Every sketch assumes TFT_eSPI is already configured for your board.

| Sketch | What it does | Hardware used | Extra library |
|---|---|---|---|
| [generative_landscape](generative_landscape/) | Draws a different mountain landscape every time you press the BOOT button — six color themes, ridges, lakes, trees, and stars. | BOOT button | none |
| [CYDSolarSystem](CYDSolarSystem/) | An animated orrery: the Sun and all eight planets in orbit, with a side panel of facts about each planet. | none | none |
| [CGCCPaint](CGCCPaint/) | A small touchscreen paint program with six colors, an adjustable brush, and a clear button. Runs in portrait. | touchscreen | XPT2046_Touchscreen |
| [FlipTimer](FlipTimer/) | A one-button stopwatch with a flip-clock display. Tap anywhere to start, stop, then reset. | touchscreen, BOOT button | XPT2046_Touchscreen |
| [MovieTerminal](MovieTerminal/) | "Computer stuff" for the background of a movie: three panes of tiny, endlessly scrolling fake code, hex, and log data. BOOT changes the colors. | BOOT button | none |
| [WiFiDetectiveDeluxe](WiFiDetectiveDeluxe/) | A handheld Wi-Fi analyzer. **Phase 1 of 6 — menu and touch navigation only so far.** | touchscreen, Wi-Fi | XPT2046_Touchscreen |

## Where to start

- **No touchscreen needed:** `generative_landscape`, `CYDSolarSystem`, and
  `MovieTerminal` only need a working display.
- **First touch sketch:** `FlipTimer` treats the whole screen as one button, so
  it works without touch calibration.
- **Touch with positions:** `CGCCPaint` needs to know where you touched. Its
  calibration values are starting estimates — see its README.
- **Larger project:** `WiFiDetectiveDeluxe` is split across several files and
  is still in progress.

## Opening a sketch

Open the `.ino` file inside the sketch's folder with **File → Open** in Arduino
IDE. Any other files in that folder open as tabs automatically. Copying and
pasting just the `.ino` into a new sketch will fail for multi-file sketches
such as `FlipTimer`, `MovieTerminal`, and `WiFiDetectiveDeluxe`.

Board: **ESP32 Dev Module**. Upload speed: **115200**.
