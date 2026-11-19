#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ---------- Touch pins (CYD) ----------
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// ---------- Touch calibration (tweak if taps land in the wrong place) ----------
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3900
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3900
#define TOUCH_INVERT_X false
#define TOUCH_INVERT_Y false
#define TOUCH_SWAP_XY  false

// ---------- Game config ----------
#define COLS        15
#define ROWS        15
#define MINES       30
#define CELL        16
#define OFFSET_X    0
#define OFFSET_Y    40
#define LONG_PRESS_MS 500

// mode button below the board (board ends at y = 40 + 15*16 = 280)
#define BTN_X       60
#define BTN_Y       287
#define BTN_W       120
#define BTN_H       28

// ---------- Colors ----------
#define C_BG        0x0000
#define C_HIDDEN    0x7BEF
#define C_REVEALED  0xC618
#define C_MINE      0xF800
#define C_FLAG      0xF800
#define C_GRID      0x4208

const uint16_t numColors[9] = {
  0x0000, 0x001F, 0x0400, 0xF800, 0x000F, 0x8000, 0x0410, 0x0000, 0x7BEF
};

// ---------- Game state ----------
struct Cell {
  bool mine;
  bool revealed;
  bool flagged;
  uint8_t adj;
};

Cell board[ROWS][COLS];
bool firstMove = true;
bool gameOver = false;
bool gameWon = false;
bool flagMode = false;
int flagsPlaced = 0;
unsigned long startTime = 0;
unsigned long lastTimerDraw = 0;

// touch state
bool touching = false;
bool longFired = false;
unsigned long touchStart = 0;
unsigned long lastTouchSeen = 0;
int touchCellX = -1, touchCellY = -1;
bool touchOnButton = false;
int lastTx = 0, lastTy = 0;

// ---------- Prototypes ----------
void newGame();
void placeMines(int safeX, int safeY);
void computeAdjacency();
void reveal(int x, int y);
void floodReveal(int x, int y);
void toggleFlag(int x, int y);
void drawCell(int x, int y);
void drawBoard();
void drawStatus();
void drawModeButton();
void checkWin();
void revealAllMines();
void handleTouch();
bool readTouch(int &sx, int &sy);

inline bool inBounds(int x, int y) {
  return x >= 0 && x < COLS && y >= 0 && y < ROWS;
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(C_BG);

  randomSeed(esp_random());
  newGame();
}

void loop() {
  handleTouch();

  if (!gameOver && !firstMove && millis() - lastTimerDraw > 1000) {
    lastTimerDraw = millis();
    drawStatus();
  }
}

// ---------- Touch reading ----------
bool readTouch(int &sx, int &sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  int rx = p.x;
  int ry = p.y;
  if (TOUCH_SWAP_XY) { int t = rx; rx = ry; ry = t; }

  sx = map(rx, TOUCH_X_MIN, TOUCH_X_MAX, 0, 240);
  sy = map(ry, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 320);
  if (TOUCH_INVERT_X) sx = 239 - sx;
  if (TOUCH_INVERT_Y) sy = 319 - sy;

  sx = constrain(sx, 0, 239);
  sy = constrain(sy, 0, 319);
  return true;
}

// ---------- Game logic ----------
void newGame() {
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++)
      board[y][x] = {false, false, false, 0};

  firstMove = true;
  gameOver = false;
  gameWon = false;
  flagMode = false;
  flagsPlaced = 0;
  startTime = 0;

  tft.fillScreen(C_BG);
  drawBoard();
  drawStatus();
  drawModeButton();
}

void placeMines(int safeX, int safeY) {
  int placed = 0;
  while (placed < MINES) {
    int x = random(COLS);
    int y = random(ROWS);
    if (abs(x - safeX) <= 1 && abs(y - safeY) <= 1) continue;
    if (board[y][x].mine) continue;
    board[y][x].mine = true;
    placed++;
  }
  computeAdjacency();
}

void computeAdjacency() {
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++) {
      int count = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          if (!dx && !dy) continue;
          int nx = x + dx, ny = y + dy;
          if (inBounds(nx, ny) && board[ny][nx].mine) count++;
        }
      board[y][x].adj = count;
    }
}

void floodReveal(int sx, int sy) {
  static int16_t stack[ROWS * COLS * 4];
  int sp = 0;
  stack[sp++] = sy * COLS + sx;

  while (sp > 0) {
    int idx = stack[--sp];
    int x = idx % COLS;
    int y = idx / COLS;

    Cell &c = board[y][x];
    if (c.revealed || c.flagged) continue;
    c.revealed = true;
    drawCell(x, y);

    if (c.adj == 0) {
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          if (!dx && !dy) continue;
          int nx = x + dx, ny = y + dy;
          if (inBounds(nx, ny) && !board[ny][nx].revealed && !board[ny][nx].flagged)
            stack[sp++] = ny * COLS + nx;
        }
    }
  }
}

void reveal(int x, int y) {
  if (gameOver) return;
  Cell &c = board[y][x];
  if (c.revealed || c.flagged) return;

  if (firstMove) {
    placeMines(x, y);
    firstMove = false;
    startTime = millis();
  }

  if (c.mine) {
    c.revealed = true;
    gameOver = true;
    gameWon = false;
    revealAllMines();
    int px = OFFSET_X + x * CELL;
    int py = OFFSET_Y + y * CELL;
    tft.fillRect(px + 1, py + 1, CELL - 1, CELL - 1, C_MINE);
    tft.fillCircle(px + CELL / 2, py + CELL / 2, 4, TFT_BLACK);
    drawStatus();
    return;
  }

  floodReveal(x, y);
  checkWin();
  drawStatus();
}

void toggleFlag(int x, int y) {
  if (gameOver) return;
  Cell &c = board[y][x];
  if (c.revealed) return;
  c.flagged = !c.flagged;
  flagsPlaced += c.flagged ? 1 : -1;
  drawCell(x, y);
  drawStatus();
}

void checkWin() {
  int hiddenSafe = 0;
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++)
      if (!board[y][x].mine && !board[y][x].revealed) hiddenSafe++;

  if (hiddenSafe == 0) {
    gameOver = true;
    gameWon = true;
    for (int y = 0; y < ROWS; y++)
      for (int x = 0; x < COLS; x++)
        if (board[y][x].mine && !board[y][x].flagged) {
          board[y][x].flagged = true;
          drawCell(x, y);
        }
    flagsPlaced = MINES;
  }
}

void revealAllMines() {
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++)
      if (board[y][x].mine && !board[y][x].flagged) {
        board[y][x].revealed = true;
        drawCell(x, y);
      }
}

// ---------- Drawing ----------
void drawCell(int x, int y) {
  Cell &c = board[y][x];
  int px = OFFSET_X + x * CELL;
  int py = OFFSET_Y + y * CELL;

  uint16_t bg = c.revealed ? C_REVEALED : C_HIDDEN;
  tft.fillRect(px, py, CELL, CELL, bg);
  tft.drawRect(px, py, CELL, CELL, C_GRID);

  if (!c.revealed) {
    tft.drawFastHLine(px + 1, py + 1, CELL - 2, TFT_WHITE);
    tft.drawFastVLine(px + 1, py + 1, CELL - 2, TFT_WHITE);
  }

  if (c.flagged && !c.revealed) {
    tft.fillTriangle(px + 5, py + 3, px + 5, py + 9, px + 12, py + 6, C_FLAG);
    tft.drawFastVLine(px + 5, py + 3, 10, TFT_BLACK);
  } else if (c.revealed && c.mine) {
    tft.fillCircle(px + CELL / 2, py + CELL / 2, 4, TFT_BLACK);
  } else if (c.revealed && c.adj > 0) {
    tft.setTextColor(numColors[c.adj], bg);
    tft.drawChar('0' + c.adj, px + 4, py + 1, 2);
  }
}

void drawBoard() {
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++)
      drawCell(x, y);
}

void drawStatus() {
  tft.fillRect(0, 0, 240, OFFSET_Y - 2, C_BG);
  tft.setTextColor(TFT_WHITE, C_BG);
  tft.drawString("Mines: " + String(MINES - flagsPlaced), 6, 4, 2);

  unsigned long secs = (firstMove || startTime == 0) ? 0 : (millis() - startTime) / 1000;
  tft.drawString("Time: " + String(secs), 140, 4, 2);

  if (gameOver) {
    if (gameWon) {
      tft.setTextColor(TFT_GREEN, C_BG);
      tft.drawString("YOU WIN! Tap to restart", 6, 22, 2);
    } else {
      tft.setTextColor(TFT_RED, C_BG);
      tft.drawString("BOOM! Tap to restart", 6, 22, 2);
    }
  }
}

void drawModeButton() {
  uint16_t col = flagMode ? TFT_ORANGE : TFT_DARKGREEN;
  tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 6, col);
  tft.drawRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 6, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, col);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(flagMode ? "MODE: FLAG" : "MODE: REVEAL", BTN_X + BTN_W / 2, BTN_Y + BTN_H / 2, 2);
  tft.setTextDatum(TL_DATUM);
}

// ---------- Touch handling ----------
void handleTouch() {
  int tx, ty;
  bool pressed = readTouch(tx, ty);
  unsigned long now = millis();

  if (pressed) {
    lastTouchSeen = now;
    lastTx = tx;
    lastTy = ty;
  }

  // treat a short gap as still touching (the XPT2046 flickers)
  bool held = pressed || (touching && now - lastTouchSeen < 60);

  if (held && !touching) {
    // touch just started
    touching = true;
    longFired = false;
    touchStart = now;
    touchOnButton = (tx >= BTN_X && tx < BTN_X + BTN_W && ty >= BTN_Y && ty < BTN_Y + BTN_H);

    touchCellX = (tx - OFFSET_X) / CELL;
    touchCellY = (ty - OFFSET_Y) / CELL;
    if (ty < OFFSET_Y || !inBounds(touchCellX, touchCellY)) {
      touchCellX = touchCellY = -1;
    }
    Serial.printf("touch screen=(%d,%d) cell=(%d,%d)\n", tx, ty, touchCellX, touchCellY);
  }
  else if (held && touching) {
    // still held: long press triggers the alternate action
    if (!longFired && !touchOnButton && touchCellX >= 0 &&
        now - touchStart > LONG_PRESS_MS && !gameOver) {
      longFired = true;
      if (flagMode) reveal(touchCellX, touchCellY);
      else toggleFlag(touchCellX, touchCellY);
    }
  }
  else if (!held && touching) {
    // released
    touching = false;

    if (gameOver) {
      if (now - touchStart > 50) newGame();
      return;
    }
    if (touchOnButton) {
      flagMode = !flagMode;
      drawModeButton();
      return;
    }
    if (!longFired && touchCellX >= 0) {
      if (flagMode) toggleFlag(touchCellX, touchCellY);
      else reveal(touchCellX, touchCellY);
    }
  }
}
