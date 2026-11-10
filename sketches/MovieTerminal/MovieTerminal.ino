// MovieTerminal -- "computer stuff" for the background of a movie scene.
//
// Three panes of tiny text scroll forever: made-up source code on the left,
// a hex dump at the top right, and log lines and numbers at the bottom right.
// None of it means anything. It just has to look busy.
//
// Press the BOOT button to change the color scheme.
//
// Needs only TFT_eSPI (set up per docs/CYD-SETUP.md). No touch, no fonts:
// the tiny font lives in TinyFont.h.

#include <TFT_eSPI.h>
#include <esp_system.h>

#include "TinyFont.h"

// ---------------------------------------------------------------- pins ----

constexpr int PIN_BACKLIGHT = 21;
constexpr int PIN_BOOT      = 0;     // BOOT button, reads LOW when pressed

// -------------------------------------------------------------- layout ----
// The screen is 320 x 240 in landscape. Text cells are 4 x 6 pixels.

constexpr int MAX_COLS = 40;                   // widest pane, in characters
constexpr int MAX_ROWS = 240 / CELL_H + 2;     // tallest pane, plus spares

constexpr int STRIP_H = 16;                    // scanlines drawn per chunk
constexpr int MAX_PANE_W = 160;

// ---------------------------------------------------------- line styles ----
// Each line of text has one style, which picks its color.

enum Style : uint8_t { NORMAL, DIM, BRIGHT, ALERT, STYLE_COUNT };

// -------------------------------------------------------------- colors ----

struct RGB {
  uint8_t r, g, b;
};

struct Theme {
  RGB code, hex, data;      // base color of each pane
};

const Theme THEMES[] = {
  {{60, 230, 110}, {70, 200, 235}, {240, 180, 60}},    // green / cyan / amber
  {{60, 230, 110}, {60, 230, 110}, {60, 230, 110}},    // all green
  {{240, 180, 60}, {240, 180, 60}, {240, 180, 60}},    // all amber
  {{120, 190, 255}, {120, 190, 255}, {120, 190, 255}}, // ice blue
};
constexpr int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);

// --------------------------------------------------------------- panes ----

// A generator writes one new line of fake text into `out` and says how it
// should be styled.
typedef Style (*Generator)(char* out, size_t size);

struct Pane {
  int x, y, w, h;           // where the pane is on screen, in pixels
  float baseSpeed;          // usual scroll speed, pixels per second
  Generator generate;

  int cols, rows;           // size of the text grid
  char text[MAX_ROWS][MAX_COLS + 1];
  Style style[MAX_ROWS];
  uint16_t colors[STYLE_COUNT];

  int top;                  // which row of text[] is currently the top line
  int offset;               // how many pixels of the top line have scrolled off
  float carry;              // leftover fraction of a pixel still to scroll
  float speed;              // current speed (changes now and then)
  uint32_t nextSpeedChange;
};

Style generateCode(char* out, size_t size);
Style generateHex(char* out, size_t size);
Style generateData(char* out, size_t size);

Pane panes[] = {
  //  x    y    w    h   speed  generator
  {   2,   0, 156, 240,  26,   generateCode},
  { 163,   0, 156, 118,  44,   generateHex},
  { 163, 121, 156, 119,  15,   generateData},
};
constexpr int PANE_COUNT = sizeof(panes) / sizeof(panes[0]);

// ------------------------------------------------------------- globals ----

TFT_eSPI tft = TFT_eSPI();

uint16_t stripBuffer[MAX_PANE_W * STRIP_H];
int themeIndex = 0;

// ------------------------------------------------- fake text: the code ----

const char* const NAME_START[] = {
  "buf", "node", "sys", "mem", "net", "key", "idx", "ptr", "reg",
  "pkt", "crc", "io", "tx", "rx", "cfg", "hash", "sig", "seq",
};
const char* const NAME_END[] = {
  "", "", "Len", "Ptr", "Id", "Mask", "Val", "Init", "Read",
  "Sync", "Lock", "Map", "Tab", "Ctl",
};

template <typename T, size_t N>
const T& pick(const T (&list)[N]) {
  return list[random(N)];
}

// Invent a plausible variable or function name, like "bufLen" or "netSync".
void makeName(char* out, size_t size) {
  snprintf(out, size, "%s%s", pick(NAME_START), pick(NAME_END));
}

Style generateCode(char* out, size_t size) {
  static int indent = 0;        // how deep inside { } we are

  char a[16], b[16], line[64];
  makeName(a, sizeof(a));
  makeName(b, sizeof(b));

  Style style = NORMAL;
  int depth = indent;           // indentation for THIS line
  int roll = random(100);

  if (indent == 0) {
    // Outside any function: start one, or write a top-level line.
    switch (random(6)) {
      case 0:
        snprintf(line, sizeof(line), "#define %s 0x%04X", a, (int)random(0x10000));
        break;
      case 1:
        snprintf(line, sizeof(line), "// %s %s", a, b);
        style = DIM;
        break;
      case 2:
        snprintf(line, sizeof(line), "static uint8_t %s[%d];", a, 8 << random(5));
        break;
      default:
        snprintf(line, sizeof(line), "%s %s(int %s) {",
                 random(2) ? "void" : "int", a, b);
        style = BRIGHT;
        indent = 1;
        break;
    }
  } else if (roll < 20) {
    // Close a block.
    indent--;
    depth = indent;
    snprintf(line, sizeof(line), "}");
  } else if (roll < 38 && indent < 4) {
    // Open a new block.
    switch (random(4)) {
      case 0:
        snprintf(line, sizeof(line), "for (i = 0; i < %s; i++) {", a);
        break;
      case 1:
        snprintf(line, sizeof(line), "if (%s(%s) != 0) {", a, b);
        break;
      case 2:
        snprintf(line, sizeof(line), "while (%s & 0x%02X) {", a, (int)random(256));
        break;
      default:
        snprintf(line, sizeof(line), "if (!%s) {", a);
        break;
    }
    indent++;
  } else {
    // An ordinary statement.
    switch (random(9)) {
      case 0:
        snprintf(line, sizeof(line), "%s = %s(%s, 0x%04X);", a, b, a, (int)random(0x10000));
        break;
      case 1:
        snprintf(line, sizeof(line), "%s[%d] ^= %s;", a, (int)random(64), b);
        break;
      case 2:
        snprintf(line, sizeof(line), "return %s;", a);
        break;
      case 3:
        snprintf(line, sizeof(line), "%s->%s = %d;", a, b, (int)random(1024));
        break;
      case 4:
        snprintf(line, sizeof(line), "// %s %s", a, b);
        style = DIM;
        break;
      case 5:
        snprintf(line, sizeof(line), "%s(&%s);", a, b);
        break;
      case 6:
        snprintf(line, sizeof(line), "%s += %s >> %d;", a, b, (int)random(1, 8));
        break;
      case 7:
        snprintf(line, sizeof(line), "uint8_t %s = 0x%02X;", a, (int)random(256));
        break;
      default:
        snprintf(line, sizeof(line), "%s |= (1 << %d);", a, (int)random(16));
        break;
    }
  }

  snprintf(out, size, "%*s%s", depth * 2, "", line);
  return style;
}

// --------------------------------------------- fake text: the hex dump ----

// One line of a memory dump: an address, eight bytes in hex, and the same
// eight bytes as characters (a dot for anything unprintable).
Style generateHex(char* out, size_t size) {
  static uint16_t address = 0x4000;

  bool empty = (random(9) == 0);      // now and then, a row of zeros
  char bytes[25], chars[9];

  for (int i = 0; i < 8; i++) {
    int value = empty ? 0 : random(256);
    snprintf(&bytes[i * 3], 4, "%02X ", value);
    chars[i] = (value >= 33 && value <= 126) ? (char)value : '.';
  }
  chars[8] = '\0';

  snprintf(out, size, "%04X: %s%s", address, bytes, chars);
  address += 8;
  return empty ? DIM : NORMAL;
}

// ------------------------------------------------ fake text: the data ----

const char* const UNIT[]  = {"NODE", "LINK", "CORE", "PORT", "GATE", "CELL"};
const char* const EVENT[] = {"SYNC OK", "ACK", "HANDSHAKE", "AUTH VALID",
                             "ROUTE SET", "CACHE HIT", "KEY ROTATE", "IDLE"};
const char* const ALARM[] = {"TEMP HIGH", "CRC FAIL", "TIMEOUT", "RETRY"};

Style generateData(char* out, size_t size) {
  static uint32_t clock = 43200;      // a pretend time of day, in seconds
  clock += random(1, 4);

  int hours = (clock / 3600) % 24;
  int minutes = (clock / 60) % 60;
  int seconds = clock % 60;

  switch (random(10)) {
    case 0: {                         // four columns of decimals
      int n = 0;
      for (int i = 0; i < 4 && n < (int)size; i++) {
        n += snprintf(out + n, size - n, "%3d.%03d  ",
                      (int)random(-99, 1000), (int)random(1000));
      }
      return NORMAL;
    }
    case 1: {                         // a row of binary
      int n = 0;
      for (int i = 0; i < 4 && n < (int)size; i++) {
        int value = random(256);
        for (int bit = 7; bit >= 0; bit--) {
          out[n++] = (value >> bit) & 1 ? '1' : '0';
        }
        out[n++] = ' ';
      }
      out[n] = '\0';
      return DIM;
    }
    case 2:                           // a position
      snprintf(out, size, "LAT %2d.%04d LON -%d.%04d ALT %d",
               (int)random(10, 60), (int)random(10000),
               (int)random(70, 125), (int)random(10000), (int)random(50, 900));
      return NORMAL;
    case 3: {                         // a progress bar
      int percent = random(101);
      char bar[21];
      for (int i = 0; i < 20; i++) bar[i] = (i * 5 < percent) ? '#' : '.';
      bar[20] = '\0';
      snprintf(out, size, "LOAD [%s] %3d%%", bar, percent);
      return BRIGHT;
    }
    case 4:                           // something going wrong
      snprintf(out, size, "[%02d:%02d:%02d] WARN %s %02d %s",
               hours, minutes, seconds, pick(UNIT), (int)random(32), pick(ALARM));
      return ALERT;
    default:                          // an ordinary log line
      snprintf(out, size, "[%02d:%02d:%02d] %s %02d %s 0x%04X",
               hours, minutes, seconds, pick(UNIT), (int)random(32),
               pick(EVENT), (int)random(0x10000));
      return NORMAL;
  }
}

// ------------------------------------------------------- pane plumbing ----

// Put a freshly generated line into row `row` of the pane's text grid.
// The row is padded with spaces so every row is exactly `cols` long.
void fillRow(Pane& pane, int row) {
  char line[80];
  line[0] = '\0';
  pane.style[row] = pane.generate(line, sizeof(line));

  int length = strlen(line);
  for (int col = 0; col < pane.cols; col++) {
    pane.text[row][col] = (col < length) ? line[col] : ' ';
  }
  pane.text[row][pane.cols] = '\0';
}

// Work out the four style colors for a pane from its base color.
void setPaneColor(Pane& pane, RGB base) {
  pane.colors[NORMAL] = tft.color565(base.r, base.g, base.b);
  pane.colors[DIM]    = tft.color565(base.r * 45 / 100, base.g * 45 / 100, base.b * 45 / 100);
  pane.colors[BRIGHT] = tft.color565((base.r + 255 * 2) / 3, (base.g + 255 * 2) / 3,
                                     (base.b + 255 * 2) / 3);
  pane.colors[ALERT]  = tft.color565(255, 70, 50);
}

// Draw the whole pane.
//
// We do not ask the display to draw letters. Instead we build the picture
// ourselves, one horizontal line of pixels (a "scanline") at a time, and send
// it in chunks. For each scanline we work out which line of text it crosses
// and which of that text's 6 pixel rows it is, then copy the font's dots.
// Scrolling by one pixel is just starting that calculation one pixel later.
void drawPane(const Pane& pane) {
  for (int stripY = 0; stripY < pane.h; stripY += STRIP_H) {
    int stripH = pane.h - stripY;
    if (stripH > STRIP_H) stripH = STRIP_H;

    for (int line = 0; line < stripH; line++) {
      uint16_t* out = &stripBuffer[line * pane.w];
      memset(out, 0, pane.w * sizeof(uint16_t));       // start all black

      int contentY = stripY + line + pane.offset;      // position in the text
      int glyphY = contentY % CELL_H;                  // row within the cell
      if (glyphY >= GLYPH_H) continue;                 // the blank spacing row

      int row = (pane.top + contentY / CELL_H) % pane.rows;
      const char* text = pane.text[row];
      uint16_t color = pane.colors[pane.style[row]];

      for (int col = 0; col < pane.cols; col++, out += CELL_W) {
        int dots = glyphRow(text[col], glyphY);
        if (dots & 0b100) out[0] = color;
        if (dots & 0b010) out[1] = color;
        if (dots & 0b001) out[2] = color;
      }
    }
    tft.pushImage(pane.x, pane.y + stripY, pane.w, stripH, stripBuffer);
  }
}

// Now and then each pane changes pace: usually near its normal speed,
// sometimes a short pause, sometimes a burst. Real output is never steady.
void chooseSpeed(Pane& pane, uint32_t now) {
  int roll = random(100);
  if (roll < 12) {
    pane.speed = 0;
    pane.nextSpeedChange = now + random(300, 900);
  } else if (roll < 30) {
    pane.speed = pane.baseSpeed * 2.5f;
    pane.nextSpeedChange = now + random(500, 1500);
  } else {
    pane.speed = pane.baseSpeed * random(70, 131) / 100.0f;
    pane.nextSpeedChange = now + random(1500, 5000);
  }
}

// Scroll a pane by however far it should have moved in `seconds`.
void updatePane(Pane& pane, uint32_t now, float seconds) {
  if (now >= pane.nextSpeedChange) {
    chooseSpeed(pane, now);
  }

  pane.carry += pane.speed * seconds;
  int pixels = (int)pane.carry;       // whole pixels to move this time
  if (pixels == 0) return;
  pane.carry -= pixels;

  pane.offset += pixels;
  while (pane.offset >= CELL_H) {
    // The top line has scrolled completely off. Reuse its row for a brand
    // new line at the bottom -- this is what makes the scroll endless.
    pane.offset -= CELL_H;
    fillRow(pane, pane.top);
    pane.top = (pane.top + 1) % pane.rows;
  }
  drawPane(pane);
}

// ------------------------------------------------------------- themes ----

void applyTheme() {
  const Theme& theme = THEMES[themeIndex];
  setPaneColor(panes[0], theme.code);
  setPaneColor(panes[1], theme.hex);
  setPaneColor(panes[2], theme.data);

  // Thin divider lines between the panes.
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(160, 0, 1, 240, panes[0].colors[DIM]);
  tft.fillRect(161, 119, 159, 1, panes[1].colors[DIM]);

  for (int i = 0; i < PANE_COUNT; i++) {
    drawPane(panes[i]);
  }
}

// True once each time the BOOT button goes down.
bool buttonPressed() {
  static bool wasDown = false;
  static uint32_t lastChange = 0;

  bool down = (digitalRead(PIN_BOOT) == LOW);
  uint32_t now = millis();
  if (down != wasDown && now - lastChange > 40) {   // ignore contact bounce
    wasDown = down;
    lastChange = now;
    return down;
  }
  return false;
}

// ---------------------------------------------------------- setup / loop ----

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  randomSeed(esp_random());           // different nonsense every power-up

  tft.init();
  tft.setRotation(1);                 // landscape, 320 x 240
  tft.setSwapBytes(true);             // so pushImage() colors come out right

  for (int i = 0; i < PANE_COUNT; i++) {
    Pane& pane = panes[i];
    pane.cols = pane.w / CELL_W;
    pane.rows = pane.h / CELL_H + 2;
    pane.top = 0;
    pane.offset = 0;
    pane.carry = 0;
    pane.speed = pane.baseSpeed;
    pane.nextSpeedChange = 0;
    for (int row = 0; row < pane.rows; row++) {
      fillRow(pane, row);             // start with a full screen of text
    }
  }

  applyTheme();
  Serial.println("MovieTerminal ready");
}

void loop() {
  static uint32_t lastFrame = millis();

  uint32_t now = millis();
  float seconds = (now - lastFrame) / 1000.0f;
  lastFrame = now;
  if (seconds > 0.1f) seconds = 0.1f;   // never jump a long way in one go

  if (buttonPressed()) {
    themeIndex = (themeIndex + 1) % THEME_COUNT;
    applyTheme();
  }

  for (int i = 0; i < PANE_COUNT; i++) {
    updatePane(panes[i], now, seconds);
  }
}
