// FlipTimer -- a one-button stopwatch with a flip-clock display.
//
// Four flip cards show minutes and seconds (MM:SS). There is exactly one
// button, and it cycles:   Start  ->  Stop  ->  Reset  ->  Start ...
//
// "The button" is a tap ANYWHERE on the touchscreen, or the BOOT button on
// the back of the board. Because the whole screen is one button, the sketch
// never needs to know where you touched -- so no touch calibration.
//
// Needs: TFT_eSPI (set up per docs/CYD-SETUP.md) and XPT2046_Touchscreen.

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

#include "FlipDigits.h"

// ---------------------------------------------------------------- pins ----

constexpr int PIN_BACKLIGHT = 21;
constexpr int PIN_BOOT      = 0;     // BOOT button, reads LOW when pressed

constexpr int PIN_TOUCH_CLK  = 25;   // the touch chip has its own SPI bus
constexpr int PIN_TOUCH_CS   = 33;
constexpr int PIN_TOUCH_MOSI = 32;
constexpr int PIN_TOUCH_MISO = 39;
constexpr int PIN_TOUCH_IRQ  = 36;

// -------------------------------------------------------------- layout ----
// The screen is 320 x 240 in landscape.

constexpr int CARD_W      = 62;
constexpr int CARD_H      = 100;
constexpr int CARD_Y      = 28;
constexpr int CARD_RADIUS = 6;       // rounded outer corners

// Each card is two halves with a 2-pixel dark gap between them -- the hinge.
constexpr int HALF_H   = 49;         // rows 0..48 are the top half
constexpr int BOTTOM_Y = 51;         // rows 51..99 are the bottom half

// Left edge of each card: two minute digits, the colon, two second digits.
const int CARD_X[4] = {17, 85, 172, 240};
constexpr int COLON_X = 159;

// Where the digit picture sits inside a card.
constexpr int DIGIT_X = (CARD_W - DIGIT_W) / 2;
constexpr int DIGIT_Y = (CARD_H - DIGIT_H) / 2;

constexpr int BUTTON_X = 60;
constexpr int BUTTON_Y = 164;
constexpr int BUTTON_W = 200;
constexpr int BUTTON_H = 52;

// -------------------------------------------------------------- timing ----

constexpr uint32_t FLIP_MS     = 300;    // how long one card flip takes
constexpr uint32_t DEBOUNCE_MS = 40;     // ignore input flicker shorter than this
constexpr uint32_t MAX_SECONDS = 99 * 60 + 59;   // the display tops out at 99:59

// -------------------------------------------------------------- colors ----

struct RGB {
  uint8_t r, g, b;
};

const RGB COLOR_DIGIT       = {242, 240, 232};
const RGB COLOR_CARD_TOP    = {24, 24, 24};
const RGB COLOR_CARD_BOTTOM = {32, 32, 32};   // a touch lighter, like real cards

// ------------------------------------------------------------- globals ----

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// The ten digit pictures, built once in setup(). One byte per pixel:
// 0 = card background, 255 = digit.
uint8_t digitPixels[10][DIGIT_W * DIGIT_H];

// Color lookup tables: digit-picture byte (0..255) -> screen color.
uint16_t topColors[256];        // top half of a card at rest
uint16_t bottomColors[256];     // bottom half of a card at rest
uint16_t flapColors[256];       // the moving flap, rebuilt as its shading changes

// One half of a card is assembled here, then sent to the screen in one go.
uint16_t halfBuffer[CARD_W * HALF_H];

struct Card {
  int x;                // left edge on screen
  int shown;            // digit currently displayed
  int target;           // digit we are flipping to
  bool flipping;
  bool topFinished;     // true once the top half has finished its part
  uint32_t flipStart;   // millis() when the flip began
};

Card cards[4];

enum TimerState { IDLE, RUNNING, STOPPED };

TimerState state = IDLE;
uint32_t startMillis = 0;       // millis() when Start was pressed
uint32_t shownSeconds = 0;      // the time currently on the cards

// ---------------------------------------------------- drawing the cards ----

// Fill a lookup table that fades from `background` to the digit color.
// `brightness` (0..255) darkens the whole thing, for shading a tilted flap.
void buildColorTable(uint16_t* table, RGB background, int brightness) {
  for (int a = 0; a < 256; a++) {
    int r = background.r + (COLOR_DIGIT.r - background.r) * a / 255;
    int g = background.g + (COLOR_DIGIT.g - background.g) * a / 255;
    int b = background.b + (COLOR_DIGIT.b - background.b) * a / 255;
    table[a] = tft.color565(r * brightness / 255,
                            g * brightness / 255,
                            b * brightness / 255);
  }
}

// How many pixels to trim from each end of card row `y` to round the corners.
int cornerInset(int y) {
  float dy;
  if (y < CARD_RADIUS) {
    dy = CARD_RADIUS - y - 0.5f;
  } else if (y >= CARD_H - CARD_RADIUS) {
    dy = y - (CARD_H - CARD_RADIUS) + 0.5f;
  } else {
    return 0;
  }
  return (int)(CARD_RADIUS - sqrtf(CARD_RADIUS * CARD_RADIUS - dy * dy) + 0.5f);
}

// Write one row (CARD_W pixels) of the card showing `digit` into `out`.
// `y` is the row within the card, 0 at the top.
void cardRow(int digit, int y, const uint16_t* colors, uint16_t* out) {
  int inset = cornerInset(y);
  int digitRow = y - DIGIT_Y;
  bool rowHasDigit = (digitRow >= 0 && digitRow < DIGIT_H);

  for (int x = 0; x < CARD_W; x++) {
    if (x < inset || x >= CARD_W - inset) {
      out[x] = TFT_BLACK;               // outside the rounded corner
      continue;
    }
    int digitCol = x - DIGIT_X;
    uint8_t a = 0;
    if (rowHasDigit && digitCol >= 0 && digitCol < DIGIT_W) {
      a = digitPixels[digit][digitRow * DIGIT_W + digitCol];
    }
    out[x] = colors[a];
  }
}

// Draw the top half of a card.
//
// The half shows `backDigit`. On top of that, the lowest `flapHeight` rows are
// covered by a flap showing the top half of `flapDigit`, squashed to fit.
// A flap squashed toward the hinge is what a card looks like as it tips
// forward and we see it more and more edge-on.
void drawTopHalf(const Card& card, int backDigit, int flapDigit, int flapHeight) {
  int flapTop = HALF_H - flapHeight;
  for (int row = 0; row < HALF_H; row++) {
    uint16_t* out = &halfBuffer[row * CARD_W];
    if (row >= flapTop) {
      int sourceRow = (row - flapTop) * HALF_H / flapHeight;
      cardRow(flapDigit, sourceRow, flapColors, out);
    } else {
      cardRow(backDigit, row, topColors, out);
    }
  }
  tft.pushImage(card.x, CARD_Y, CARD_W, HALF_H, halfBuffer);
}

// Draw the bottom half of a card: `backDigit` underneath, with a flap showing
// the bottom half of `flapDigit` growing downward from the hinge.
void drawBottomHalf(const Card& card, int backDigit, int flapDigit, int flapHeight) {
  for (int row = 0; row < HALF_H; row++) {
    uint16_t* out = &halfBuffer[row * CARD_W];
    if (row < flapHeight) {
      int sourceRow = BOTTOM_Y + row * HALF_H / flapHeight;
      cardRow(flapDigit, sourceRow, flapColors, out);
    } else {
      cardRow(backDigit, BOTTOM_Y + row, bottomColors, out);
    }
  }
  tft.pushImage(card.x, CARD_Y + BOTTOM_Y, CARD_W, HALF_H, halfBuffer);
}

// Draw a whole card at rest, no flap.
void drawCard(const Card& card) {
  drawTopHalf(card, card.shown, card.shown, 0);
  drawBottomHalf(card, card.shown, card.shown, 0);
}

// ------------------------------------------------------- flip animation ----
//
// A flip has two parts:
//   first half  -- the old top half tips forward and shrinks toward the hinge,
//                  uncovering the new digit's top half behind it.
//   second half -- the flap continues down past the hinge, now showing the new
//                  digit's bottom half, and grows until it covers the old one.

void startFlip(Card& card, int digit) {
  if (card.flipping) {            // already mid-flip: finish that one instantly
    card.shown = card.target;
    card.flipping = false;
    drawCard(card);
  }
  if (digit == card.shown) return;

  card.target = digit;
  card.flipping = true;
  card.topFinished = false;
  card.flipStart = millis();
}

void updateFlip(Card& card) {
  if (!card.flipping) return;

  float progress = (float)(millis() - card.flipStart) / FLIP_MS;   // 0 -> 1

  if (progress < 0.5f) {
    // Flap tips from upright (0 degrees) to edge-on (90 degrees).
    float angle = progress * 2 * (PI / 2);
    int flapHeight = (int)(HALF_H * cosf(angle) + 0.5f);
    buildColorTable(flapColors, COLOR_CARD_TOP, 255 - (int)(110 * progress * 2));
    drawTopHalf(card, card.target, card.shown, flapHeight);
    return;
  }

  if (!card.topFinished) {        // top half is now fully the new digit
    drawTopHalf(card, card.target, card.target, 0);
    card.topFinished = true;
  }

  if (progress >= 1.0f) {         // flip complete
    card.shown = card.target;
    card.flipping = false;
    drawBottomHalf(card, card.shown, card.shown, 0);
    return;
  }

  // Flap swings from edge-on down to flat against the bottom half.
  float swing = (progress - 0.5f) * 2;                 // 0 -> 1
  int flapHeight = (int)(HALF_H * sinf(swing * (PI / 2)) + 0.5f);
  buildColorTable(flapColors, COLOR_CARD_BOTTOM, 145 + (int)(110 * swing));
  drawBottomHalf(card, card.shown, card.target, flapHeight);
}

// Put a time on the cards. Only cards whose digit changes will flip.
void showTime(uint32_t seconds) {
  shownSeconds = seconds;
  uint32_t minutes = seconds / 60;
  uint32_t secs = seconds % 60;
  startFlip(cards[0], minutes / 10);
  startFlip(cards[1], minutes % 10);
  startFlip(cards[2], secs / 10);
  startFlip(cards[3], secs % 10);
}

// ----------------------------------------------------------- the button ----

// The button always shows what the NEXT press will do.
void drawButton() {
  const char* label = "Start";
  uint16_t color = tft.color565(93, 202, 165);        // green

  if (state == RUNNING) {
    label = "Stop";
    color = tft.color565(240, 149, 149);              // red
  } else if (state == STOPPED) {
    label = "Reset";
    color = tft.color565(250, 199, 117);              // amber
  }

  tft.fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 10, color);
  tft.setTextDatum(MC_DATUM);                         // center the text
  tft.setTextColor(TFT_BLACK, color);
  tft.drawString(label, BUTTON_X + BUTTON_W / 2, BUTTON_Y + BUTTON_H / 2, 4);
}

// Returns true once per press -- at the moment a finger (or the BOOT button)
// goes down. Holding does nothing more; you must let go and press again.
//
// Touchscreens and buttons "bounce": the signal flickers for a few
// milliseconds as contact is made or lost. We only believe a change after the
// input has stayed in its new state for DEBOUNCE_MS.
bool buttonPressed() {
  static bool pressed = false;          // the debounced state
  static bool lastReading = false;
  static uint32_t lastChange = 0;

  bool reading = touch.touched() || digitalRead(PIN_BOOT) == LOW;
  uint32_t now = millis();

  if (reading != lastReading) {
    lastReading = reading;
    lastChange = now;
  }

  if (reading != pressed && now - lastChange >= DEBOUNCE_MS) {
    pressed = reading;
    return pressed;                     // true only on the way down
  }
  return false;
}

// One press moves to the next state: Start -> Stop -> Reset -> Start ...
void handlePress() {
  switch (state) {
    case IDLE:
      startMillis = millis();
      state = RUNNING;
      break;
    case RUNNING:
      state = STOPPED;                  // the cards simply stay where they are
      break;
    case STOPPED:
      showTime(0);
      state = IDLE;
      break;
  }
  drawButton();
}

// ---------------------------------------------------------- setup / loop ----

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);                   // landscape, 320 x 240
  tft.setSwapBytes(true);               // so pushImage() colors come out right
  tft.fillScreen(TFT_BLACK);

  touchSPI.begin(PIN_TOUCH_CLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  touch.begin(touchSPI);

  for (int d = 0; d < 10; d++) {
    renderDigit(d, digitPixels[d]);
  }
  buildColorTable(topColors, COLOR_CARD_TOP, 255);
  buildColorTable(bottomColors, COLOR_CARD_BOTTOM, 255);

  for (int i = 0; i < 4; i++) {
    cards[i] = {CARD_X[i], 0, 0, false, false, 0};
    drawCard(cards[i]);
  }

  // The colon: two dots between the minutes and the seconds.
  uint16_t dotColor = tft.color565(COLOR_DIGIT.r, COLOR_DIGIT.g, COLOR_DIGIT.b);
  tft.fillCircle(COLON_X, CARD_Y + 34, 3, dotColor);
  tft.fillCircle(COLON_X, CARD_Y + 66, 3, dotColor);

  drawButton();
  Serial.println("FlipTimer ready");
}

void loop() {
  if (buttonPressed()) {
    handlePress();
  }

  if (state == RUNNING) {
    // Work out the time from the clock every pass instead of counting ticks.
    // That way a slow animation frame can never make the stopwatch drift.
    uint32_t seconds = (millis() - startMillis) / 1000;

    if (seconds >= MAX_SECONDS) {       // out of digits: stop at 99:59
      seconds = MAX_SECONDS;
      state = STOPPED;
      drawButton();
    }
    if (seconds != shownSeconds) {
      showTime(seconds);
    }
  }

  for (int i = 0; i < 4; i++) {
    updateFlip(cards[i]);
  }
}
