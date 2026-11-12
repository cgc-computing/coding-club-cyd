// FiveDice -- a solo dice game in the Yacht family for the touchscreen.
//
// Each turn you get up to three rolls of five dice. After a roll, tap a die
// to HOLD it (it will not be re-rolled). When you are happy -- or out of
// rolls -- tap a row on the scorecard to pick it, then tap it again to lock
// the score in. Every row can be used once. Thirteen turns fill the card.
//
// The first time it runs, the sketch asks you to tap three crosses so it can
// learn how your touch panel lines up with the screen. That is saved in
// flash. Press the BOOT button at any time to do it again.
//
// Needs: TFT_eSPI (set up per docs/CYD-SETUP.md) and XPT2046_Touchscreen.

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Preferences.h>

// ---------------------------------------------------------------- pins ----

constexpr int PIN_BACKLIGHT = 21;
constexpr int PIN_BOOT      = 0;     // BOOT button, reads LOW when pressed

constexpr int PIN_TOUCH_CLK  = 25;   // the touch chip has its own SPI bus
constexpr int PIN_TOUCH_CS   = 33;
constexpr int PIN_TOUCH_MOSI = 32;
constexpr int PIN_TOUCH_MISO = 39;
constexpr int PIN_TOUCH_IRQ  = 36;

// -------------------------------------------------------------- layout ----
// The screen is 320 x 240 in landscape. Dice and the ROLL button run across
// the top; the scorecard is two columns of eight rows underneath.

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

constexpr int DIE_SIZE  = 44;
constexpr int DIE_Y     = 6;
constexpr int DIE_STEP  = 50;        // distance from one die to the next
constexpr int DIE_X0    = 6;
constexpr int TOP_H     = 64;        // height of the dice strip

constexpr int ROLL_X = 258;
constexpr int ROLL_Y = 6;
constexpr int ROLL_W = 56;
constexpr int ROLL_H = 54;

constexpr int COL_X[2] = {4, 162};
constexpr int COL_W    = 154;
constexpr int ROW_Y0   = 68;
constexpr int ROW_H    = 21;
constexpr int ROWS     = 8;

// -------------------------------------------------------------- colors ----

constexpr uint16_t rgb(int r, int g, int b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t C_TABLE    = rgb(10, 60, 35);     // the green felt
constexpr uint16_t C_ROW_OPEN = rgb(24, 92, 56);
constexpr uint16_t C_ROW_USED = rgb(6, 36, 22);
constexpr uint16_t C_ROW_INFO = rgb(0, 0, 0);
constexpr uint16_t C_PICKED   = rgb(255, 210, 0);
constexpr uint16_t C_PREVIEW  = rgb(170, 230, 140);  // "you would get this"
constexpr uint16_t C_DIM      = rgb(150, 170, 155);
constexpr uint16_t C_DIE      = rgb(250, 246, 230);
constexpr uint16_t C_DIE_IDLE = rgb(90, 110, 98);    // not rolled yet
constexpr uint16_t C_HOLD     = rgb(255, 140, 0);
constexpr uint16_t C_ROLL     = rgb(200, 40, 40);
constexpr uint16_t C_ROLL_OFF = rgb(80, 80, 80);
constexpr uint16_t C_NEW      = rgb(30, 150, 60);

// ---------------------------------------------------------- the rules -----

constexpr int CATEGORIES   = 13;
constexpr int BONUS_NEEDED = 63;     // top-six total needed for the bonus
constexpr int BONUS_POINTS = 35;

// Categories 0..5 are Ones..Sixes (left column). 6..12 are the right column.
enum { THREE_KIND = 6, FOUR_KIND, FULL_HOUSE, SMALL_RUN, LARGE_RUN,
       FIVE_KIND, CHANCE };

const char* const NAMES[CATEGORIES] = {
  "Ones", "Twos", "Threes", "Fours", "Fives", "Sixes",
  "3 of Kind", "4 of Kind", "FullHouse", "Small Run", "Large Run",
  "5 of Kind", "Chance"
};

// ------------------------------------------------------------- globals ----

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
Preferences prefs;

int  dice[5];                 // 1..6, or 0 before the first roll of a turn
bool held[5];
int  rollsLeft;
int  scores[CATEGORIES];      // -1 means "not used yet"
int  picked;                  // category tapped once, waiting for 2nd tap; -1 = none
bool gameOver;
int  bestScore = 0;
bool newBest;

// How raw touch readings map onto screen pixels. Filled in by calibrate().
struct Calibration {
  bool swapXY;                // true if the panel's axes are sideways
  int16_t xLo, xHi;           // raw readings at the left and right crosses
  int16_t yLo, yHi;           // raw readings at the top and bottom crosses
};

Calibration cal;
constexpr int CAL_INSET = 24; // crosses sit this far in from the edges

// ------------------------------------------------------------- scoring ----

// What would these five dice score in `category`?
int scoreFor(int category) {
  int count[7] = {0};
  int sum = 0;
  for (int i = 0; i < 5; i++) {
    count[dice[i]]++;
    sum += dice[i];
  }

  if (category < 6) {                       // Ones..Sixes: add up that face
    int face = category + 1;
    return count[face] * face;
  }

  int most = 0;                             // biggest group of matching dice
  bool hasPair = false;
  for (int face = 1; face <= 6; face++) {
    if (count[face] > most) most = count[face];
    if (count[face] == 2) hasPair = true;
  }

  int run = 0, longestRun = 0;              // longest 1-2-3... sequence
  for (int face = 1; face <= 6; face++) {
    run = (count[face] > 0) ? run + 1 : 0;
    if (run > longestRun) longestRun = run;
  }

  switch (category) {
    case THREE_KIND: return most >= 3 ? sum : 0;
    case FOUR_KIND:  return most >= 4 ? sum : 0;
    case FULL_HOUSE: return (most == 3 && hasPair) ? 25 : 0;
    case SMALL_RUN:  return longestRun >= 4 ? 30 : 0;
    case LARGE_RUN:  return longestRun >= 5 ? 40 : 0;
    case FIVE_KIND:  return most == 5 ? 50 : 0;
    default:         return sum;            // Chance
  }
}

int upperTotal() {
  int total = 0;
  for (int c = 0; c < 6; c++) {
    if (scores[c] > 0) total += scores[c];
  }
  return total;
}

int grandTotal() {
  int total = 0;
  for (int c = 0; c < CATEGORIES; c++) {
    if (scores[c] > 0) total += scores[c];
  }
  if (upperTotal() >= BONUS_NEEDED) total += BONUS_POINTS;
  return total;
}

// ------------------------------------------------------------- drawing ----

void drawDie(int i) {
  int x = DIE_X0 + i * DIE_STEP;
  int v = dice[i];
  uint16_t face = (v == 0) ? C_DIE_IDLE : C_DIE;

  // A held die gets an orange rim and a label underneath.
  tft.fillRoundRect(x, DIE_Y, DIE_SIZE, DIE_SIZE, 7, held[i] ? C_HOLD : face);
  tft.fillRoundRect(x + 3, DIE_Y + 3, DIE_SIZE - 6, DIE_SIZE - 6, 5, face);

  // Pips sit on a 3 x 3 grid.
  int a = 11, b = 22, c = 33;
  auto pip = [&](int px, int py) {
    tft.fillCircle(x + px, DIE_Y + py, 4, TFT_BLACK);
  };
  if (v % 2 == 1) pip(b, b);
  if (v >= 2) { pip(a, a); pip(c, c); }
  if (v >= 4) { pip(c, a); pip(a, c); }
  if (v == 6) { pip(a, b); pip(c, b); }

  tft.fillRect(x, DIE_Y + DIE_SIZE + 2, DIE_SIZE, 9, C_TABLE);
  if (held[i]) {
    tft.setTextSize(1);
    tft.setTextColor(C_HOLD, C_TABLE);
    tft.setCursor(x + 10, DIE_Y + DIE_SIZE + 3);
    tft.print("HOLD");
  }
}

void drawRollButton() {
  uint16_t color = gameOver ? C_NEW : (rollsLeft > 0 ? C_ROLL : C_ROLL_OFF);
  tft.fillRoundRect(ROLL_X, ROLL_Y, ROLL_W, ROLL_H, 6, color);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, color);

  if (gameOver) {
    tft.setCursor(ROLL_X + 10, ROLL_Y + 8);
    tft.print("NEW");
    tft.setCursor(ROLL_X + 4, ROLL_Y + 30);
    tft.print("GAME");
    return;
  }

  tft.setCursor(ROLL_X + 4, ROLL_Y + 8);
  tft.print("ROLL");
  for (int i = 0; i < 3; i++) {             // one dot per roll remaining
    int cx = ROLL_X + 14 + i * 14;
    if (i < rollsLeft) {
      tft.fillCircle(cx, ROLL_Y + 39, 5, TFT_WHITE);
    } else {
      tft.drawCircle(cx, ROLL_Y + 39, 5, TFT_WHITE);
    }
  }
}

// Draw one scorecard row: a name on the left, a value on the right.
void drawRowBox(int col, int row, const char* name, const char* value,
                uint16_t background, uint16_t nameColor, uint16_t valueColor) {
  int x = COL_X[col];
  int y = ROW_Y0 + row * ROW_H;
  tft.fillRect(x, y, COL_W, ROW_H - 1, background);
  tft.setTextSize(2);                       // each letter is 12 x 16 pixels
  tft.setTextColor(nameColor, background);
  tft.setCursor(x + 4, y + 2);
  tft.print(name);
  tft.setTextColor(valueColor, background);
  tft.setCursor(x + COL_W - 4 - 12 * (int)strlen(value), y + 2);
  tft.print(value);
}

void drawCategory(int c) {
  int col = (c < 6) ? 0 : 1;
  int row = (c < 6) ? c : c - 6;
  char value[16] = "";

  if (scores[c] >= 0) {                     // already used
    snprintf(value, sizeof(value), "%d", scores[c]);
    drawRowBox(col, row, NAMES[c], value, C_ROW_USED, C_DIM, TFT_WHITE);
  } else if (dice[0] == 0) {                // open, but nothing rolled yet
    drawRowBox(col, row, NAMES[c], value, C_ROW_OPEN, TFT_WHITE, TFT_WHITE);
  } else {                                  // open: preview what it would score
    snprintf(value, sizeof(value), "%d", scoreFor(c));
    if (c == picked) {
      drawRowBox(col, row, NAMES[c], value, C_PICKED, TFT_BLACK, TFT_BLACK);
    } else {
      drawRowBox(col, row, NAMES[c], value, C_ROW_OPEN, TFT_WHITE, C_PREVIEW);
    }
  }
}

// The three rows that are not categories: bonus progress, best score, total.
void drawTotals() {
  char value[16];

  int upper = upperTotal();
  if (upper >= BONUS_NEEDED) {
    snprintf(value, sizeof(value), "+%d", BONUS_POINTS);
  } else {
    snprintf(value, sizeof(value), "%d/%d", upper, BONUS_NEEDED);
  }
  drawRowBox(0, 6, "Bonus", value, C_ROW_INFO, C_DIM, TFT_WHITE);

  snprintf(value, sizeof(value), "%d", bestScore);
  if (newBest) {
    drawRowBox(0, 7, "NEW BEST!", value, C_PICKED, TFT_BLACK, TFT_BLACK);
  } else {
    drawRowBox(0, 7, "Best", value, C_ROW_INFO, C_DIM, TFT_WHITE);
  }

  snprintf(value, sizeof(value), "%d", grandTotal());
  drawRowBox(1, 7, "TOTAL", value, C_ROW_INFO, TFT_YELLOW, TFT_YELLOW);
}

void drawOpenCategories() {
  for (int c = 0; c < CATEGORIES; c++) {
    if (scores[c] < 0) drawCategory(c);
  }
}

void drawEverything() {
  tft.fillScreen(C_TABLE);
  for (int i = 0; i < 5; i++) drawDie(i);
  drawRollButton();
  for (int c = 0; c < CATEGORIES; c++) drawCategory(c);
  drawTotals();
}

// --------------------------------------------------------------- touch ----

// Average several raw readings while the finger is down.
bool readRaw(int& rawX, int& rawY, int samples) {
  long sumX = 0, sumY = 0;
  int got = 0;
  for (int i = 0; i < samples; i++) {
    if (i > 0) delay(4);
    if (!touch.touched()) break;
    TS_Point p = touch.getPoint();
    sumX += p.x;
    sumY += p.y;
    got++;
  }
  if (got == 0) return false;
  rawX = sumX / got;
  rawY = sumY / got;
  return true;
}

// Returns true once per tap, when the finger first goes down, and says where.
// A touch panel flickers as contact is made, so a change only counts after it
// has held steady for 40 ms.
bool getTap(int& x, int& y) {
  static bool lastReading = false;
  static bool fingerDown = false;
  static uint32_t changedAt = 0;

  bool reading = touch.touched();
  if (reading != lastReading) {
    lastReading = reading;
    changedAt = millis();
  }
  if (millis() - changedAt < 40) return false;
  if (reading == fingerDown) return false;

  fingerDown = reading;
  if (!fingerDown) return false;

  int rawX, rawY;
  if (!readRaw(rawX, rawY, 8)) return false;

  int srcX = cal.swapXY ? rawY : rawX;
  int srcY = cal.swapXY ? rawX : rawY;
  x = map(srcX, cal.xLo, cal.xHi, CAL_INSET, SCREEN_W - 1 - CAL_INSET);
  y = map(srcY, cal.yLo, cal.yHi, CAL_INSET, SCREEN_H - 1 - CAL_INSET);
  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);
  return true;
}

void waitForRelease() {
  uint32_t clearSince = millis();
  while (millis() - clearSince < 150) {
    if (touch.touched()) clearSince = millis();
    delay(5);
  }
}

// Show a cross at (x, y), wait for a tap on it, and return the raw reading.
void calibrationTap(int x, int y, int& rawX, int& rawY) {
  tft.drawFastHLine(x - 10, y, 21, TFT_WHITE);
  tft.drawFastVLine(x, y - 10, 21, TFT_WHITE);
  waitForRelease();
  for (;;) {
    if (touch.touched()) {
      delay(120);                           // let the finger settle
      if (readRaw(rawX, rawY, 16)) break;
    }
    delay(5);
  }
  tft.drawFastHLine(x - 10, y, 21, TFT_GREEN);
  tft.drawFastVLine(x, y - 10, 21, TFT_GREEN);
}

// Ask for taps at three corners and work out the touch-to-screen mapping.
void calibrate() {
  const int left = CAL_INSET, right = SCREEN_W - 1 - CAL_INSET;
  const int top = CAL_INSET,  bottom = SCREEN_H - 1 - CAL_INSET;

  for (;;) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(52, 104);
    tft.print("Tap each white cross");

    int ax, ay, bx, by, cx, cy;
    calibrationTap(left,  top,    ax, ay);
    calibrationTap(right, top,    bx, by);
    calibrationTap(right, bottom, cx, cy);
    waitForRelease();

    // Moving left-to-right should change one raw axis a lot and the other
    // barely at all. Whichever changed is the one that means "screen X".
    cal.swapXY = abs(by - ay) > abs(bx - ax);
    if (cal.swapXY) {
      cal.xLo = ay; cal.xHi = by;
      cal.yLo = bx; cal.yHi = cx;
    } else {
      cal.xLo = ax; cal.xHi = bx;
      cal.yLo = by; cal.yHi = cy;
    }

    // A real panel spans about 3000 counts. Much less means the taps were
    // not on the crosses, so ask again.
    if (abs(cal.xHi - cal.xLo) > 400 && abs(cal.yHi - cal.yLo) > 400) break;
  }

  prefs.putBytes("cal", &cal, sizeof(cal));
}

// ------------------------------------------------------------ the game ----

void startTurn() {
  for (int i = 0; i < 5; i++) {
    dice[i] = 0;
    held[i] = false;
  }
  rollsLeft = 3;
  picked = -1;
}

void newGame() {
  for (int c = 0; c < CATEGORIES; c++) scores[c] = -1;
  gameOver = false;
  newBest = false;
  startTurn();
  drawEverything();
}

void rollDice() {
  rollsLeft--;
  picked = -1;
  drawRollButton();

  // Tumble: show a few random faces before settling on the last one.
  for (int frame = 0; frame < 8; frame++) {
    for (int i = 0; i < 5; i++) {
      if (!held[i]) {
        dice[i] = random(1, 7);
        drawDie(i);
      }
    }
    delay(45);
  }
  drawOpenCategories();                     // refresh the score previews
}

void lockInScore(int c) {
  scores[c] = scoreFor(c);

  bool cardFull = true;
  for (int i = 0; i < CATEGORIES; i++) {
    if (scores[i] < 0) cardFull = false;
  }

  startTurn();
  if (cardFull) {
    gameOver = true;
    if (grandTotal() > bestScore) {
      bestScore = grandTotal();
      newBest = true;
      prefs.putInt("best", bestScore);
    }
  }
  drawEverything();
}

void handleTap(int x, int y) {
  // --- the dice strip and ROLL button ---
  if (y < TOP_H) {
    if (x >= ROLL_X - 4) {
      if (gameOver) newGame();
      else if (rollsLeft > 0) rollDice();
      return;
    }
    // Dice can be held once they have been rolled and a roll remains.
    if (gameOver || dice[0] == 0 || rollsLeft == 0) return;
    int i = constrain((x - 3) / DIE_STEP, 0, 4);
    held[i] = !held[i];
    drawDie(i);
    return;
  }

  // --- the scorecard ---
  if (gameOver || dice[0] == 0) return;     // must roll before scoring
  int row = (y - ROW_Y0) / ROW_H;
  if (y < ROW_Y0 || row >= ROWS) return;

  int c;
  if (x < SCREEN_W / 2) {
    if (row > 5) return;                    // Bonus and Best are not tappable
    c = row;
  } else {
    if (row > 6) return;                    // neither is TOTAL
    c = row + 6;
  }
  if (scores[c] >= 0) return;               // already used

  if (c == picked) {                        // second tap: lock it in
    lockInScore(c);
  } else {                                  // first tap: highlight it
    int previous = picked;
    picked = c;
    if (previous >= 0) drawCategory(previous);
    drawCategory(c);
  }
}

// ------------------------------------------------------- setup and loop ----

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);                       // landscape

  touchSPI.begin(PIN_TOUCH_CLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);

  randomSeed(esp_random());                 // different dice every power-up

  prefs.begin("fivedice", false);
  bestScore = prefs.getInt("best", 0);
  if (prefs.getBytes("cal", &cal, sizeof(cal)) != sizeof(cal)) {
    calibrate();                            // first run: nothing saved yet
  }

  newGame();
}

void loop() {
  if (digitalRead(PIN_BOOT) == LOW) {       // BOOT = redo touch calibration
    calibrate();
    drawEverything();
  }

  int x, y;
  if (getTap(x, y)) {
    handleTap(x, y);
  }
  delay(5);
}
