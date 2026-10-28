
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// CGCC CYD Paint - Guition ESP32-2432S028

#define TFT_BL 21

#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

#define SCREEN_W 240
#define SCREEN_H 320

// Starting calibration estimates.
// Adjust after checking corner touches.
#define RAW_X_MIN 200
#define RAW_X_MAX 3900
#define RAW_Y_MIN 200
#define RAW_Y_MAX 3900

// Change these if touch movement is incorrect.
#define SWAP_XY false
#define INVERT_X false
#define INVERT_Y false

#define HEADER_H 32
#define CANVAS_TOP 36
#define CANVAS_BOTTOM 246
#define PALETTE_Y 265
#define BUTTON_Y 291

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

uint16_t colors[] = {
  TFT_WHITE, TFT_RED, TFT_YELLOW,
  TFT_GREEN, TFT_BLUE, TFT_MAGENTA
};

const int COLOR_COUNT = 6;
int selectedColor = 0;
int brushSize = 4;

bool wasTouching = false;
bool drawing = false;
int lastX = 0;
int lastY = 0;

void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H,
               TFT_NAVY);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print("CGCC PAINT");
}

void drawPalette() {
  tft.fillRect(0, 250, SCREEN_W, 38,
               TFT_DARKGREY);

  for (int i = 0; i < COLOR_COUNT; i++) {
    int cx = 20 + i * 40;

    tft.fillCircle(cx, PALETTE_Y, 12,
                   colors[i]);

    if (i == selectedColor) {
      tft.drawCircle(cx, PALETTE_Y, 15,
                     TFT_WHITE);
    }
  }
}

void drawButtons() {
  tft.fillRoundRect(8, BUTTON_Y, 108, 25,
                    5, TFT_DARKCYAN);
  tft.fillRoundRect(124, BUTTON_Y, 108, 25,
                    5, TFT_RED);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);

  tft.setCursor(22, BUTTON_Y + 9);
  tft.printf("BRUSH: %d", brushSize);

  tft.setCursor(160, BUTTON_Y + 9);
  tft.print("CLEAR");
}

void clearCanvas() {
  tft.fillRect(0, CANVAS_TOP,
               SCREEN_W,
               CANVAS_BOTTOM - CANVAS_TOP,
               TFT_BLACK);
}

void drawUI() {
  tft.fillScreen(TFT_BLACK);
  drawHeader();
  clearCanvas();
  drawPalette();
  drawButtons();
}

bool getTouch(int &x, int &y) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();

  if (p.z < 100) return false;

  int rawX = p.x;
  int rawY = p.y;

  if (SWAP_XY) {
    int temp = rawX;
    rawX = rawY;
    rawY = temp;
  }

  x = map(rawX, RAW_X_MIN, RAW_X_MAX,
          0, SCREEN_W - 1);

  y = map(rawY, RAW_Y_MIN, RAW_Y_MAX,
          0, SCREEN_H - 1);

  if (INVERT_X) x = SCREEN_W - 1 - x;
  if (INVERT_Y) y = SCREEN_H - 1 - y;

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  return true;
}

void paintAt(int x, int y) {
  // Keep the entire brush inside the canvas.
  int r = brushSize;
  x = constrain(x, r, SCREEN_W - 1 - r);
  y = constrain(y, CANVAS_TOP + r,
                CANVAS_BOTTOM - 1 - r);

  tft.fillCircle(x, y, r,
                 colors[selectedColor]);
}

void drawStroke(int x1, int y1, int x2, int y2) {
  int dx = x2 - x1;
  int dy = y2 - y1;
  int steps = max(abs(dx), abs(dy));

  if (steps == 0) {
    paintAt(x1, y1);
    return;
  }

  for (int i = 0; i <= steps; i++) {
    int x = x1 + (long)dx * i / steps;
    int y = y1 + (long)dy * i / steps;
    paintAt(x, y);
  }
}

void handleTouch(int x, int y) {
  if (y >= CANVAS_TOP && y < CANVAS_BOTTOM) {
    if (drawing) {
      drawStroke(lastX, lastY, x, y);
    } else {
      paintAt(x, y);
    }

    lastX = x;
    lastY = y;
    drawing = true;
    return;
  }

  drawing = false;

  // Only trigger controls on a new touch.
  if (wasTouching) return;

  if (y >= 250 && y < 285) {
    int index = (x + 0) / 40;

    if (index >= 0 && index < COLOR_COUNT) {
      selectedColor = index;
      drawPalette();
    }
  }

  if (y >= BUTTON_Y) {
    if (x < 120) {
      brushSize += 2;
      if (brushSize > 10) brushSize = 2;
      drawButtons();
    } else {
      clearCanvas();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(0);

  touchSPI.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  ts.begin(touchSPI);
  ts.setRotation(0);

  drawUI();

  Serial.println("CGCC CYD Paint ready!");
}

void loop() {
  int x, y;

  if (getTouch(x, y)) {
    handleTouch(x, y);

    Serial.printf("X=%d Y=%d\n", x, y);
    wasTouching = true;
  } else {
    wasTouching = false;
    drawing = false;
  }

  delay(10);
}
