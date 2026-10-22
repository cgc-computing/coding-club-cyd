#include <TFT_eSPI.h>
#include <esp_system.h>

TFT_eSPI tft = TFT_eSPI();

// ======================================================
// Hardware
// ======================================================

constexpr int TFT_BACKLIGHT = 21;
constexpr int BOOT_BUTTON   = 0;

// ======================================================
// Screen
// ======================================================

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

// ======================================================
// Theme
// ======================================================

struct Theme {
  uint16_t skyTop;
  uint16_t skyBottom;
  uint16_t celestial;
  uint16_t cloud;
  uint16_t mountainFar;
  uint16_t mountainMid;
  uint16_t mountainNear;
  uint16_t ground;
  uint16_t tree;
  uint16_t snow;
  uint16_t water;
  uint16_t reflection;
  uint16_t text;
  uint16_t panel;
  bool night;
};

Theme theme;

// ======================================================
// Ridge data
// ======================================================

constexpr int MAX_POINTS = 24;

struct Ridge {
  int count;
  int xs[MAX_POINTS];
  int ys[MAX_POINTS];
};

Ridge farRidge;
Ridge midRidge;
Ridge nearRidge;
Ridge foregroundRidge;

// ======================================================
// World state
// ======================================================

uint32_t worldSeed = 0;

bool hasLake = false;

int celestialX = 0;
int celestialY = 0;
int celestialR = 0;

// ======================================================
// Button state
// ======================================================

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;
unsigned long buttonDownTime = 0;

constexpr unsigned long DEBOUNCE_MS = 40;
constexpr unsigned long HOLD_MS = 700;

// ======================================================
// Color helpers
// ======================================================

uint8_t red565(uint16_t c) {
  return ((c >> 11) & 0x1F) << 3;
}

uint8_t green565(uint16_t c) {
  return ((c >> 5) & 0x3F) << 2;
}

uint8_t blue565(uint16_t c) {
  return (c & 0x1F) << 3;
}

uint16_t blend565(uint16_t c1, uint16_t c2, float t) {
  t = constrain(t, 0.0f, 1.0f);

  uint8_t r =
    red565(c1) +
    (red565(c2) - red565(c1)) * t;

  uint8_t g =
    green565(c1) +
    (green565(c2) - green565(c1)) * t;

  uint8_t b =
    blue565(c1) +
    (blue565(c2) - blue565(c1)) * t;

  return tft.color565(r, g, b);
}

// ======================================================
// Random helpers
// ======================================================

int randRange(int lowVal, int highVal) {
  return random(lowVal, highVal + 1);
}

bool chance(int percent) {
  return random(100) < percent;
}

// ======================================================
// Theme selection
// ======================================================

void chooseTheme() {

  int style = random(6);

  switch (style) {

    // Blue alpine
    case 0:
      theme.skyTop =
        tft.color565(25, 115, 245);

      theme.skyBottom =
        tft.color565(175, 225, 255);

      theme.celestial =
        tft.color565(255, 225, 80);

      theme.cloud =
        tft.color565(248, 248, 250);

      theme.mountainFar =
        tft.color565(120, 165, 205);

      theme.mountainMid =
        tft.color565(75, 120, 165);

      theme.mountainNear =
        tft.color565(45, 80, 120);

      theme.ground =
        tft.color565(40, 100, 60);

      theme.tree =
        tft.color565(15, 65, 35);

      theme.snow =
        tft.color565(245, 248, 250);

      theme.water =
        tft.color565(30, 105, 160);

      theme.reflection =
        tft.color565(150, 210, 220);

      theme.text =
        TFT_WHITE;

      theme.panel =
        TFT_BLACK;

      theme.night = false;
      break;

    // Sunset
    case 1:
      theme.skyTop =
        tft.color565(100, 35, 145);

      theme.skyBottom =
        tft.color565(255, 130, 65);

      theme.celestial =
        tft.color565(255, 215, 105);

      theme.cloud =
        tft.color565(255, 190, 185);

      theme.mountainFar =
        tft.color565(155, 100, 165);

      theme.mountainMid =
        tft.color565(110, 65, 135);

      theme.mountainNear =
        tft.color565(70, 40, 95);

      theme.ground =
        tft.color565(65, 70, 55);

      theme.tree =
        tft.color565(25, 40, 30);

      theme.snow =
        tft.color565(255, 225, 220);

      theme.water =
        tft.color565(105, 45, 100);

      theme.reflection =
        tft.color565(255, 150, 90);

      theme.text =
        TFT_WHITE;

      theme.panel =
        tft.color565(12, 7, 15);

      theme.night = false;
      break;

    // Pink fantasy
    case 2:
      theme.skyTop =
        tft.color565(65, 25, 120);

      theme.skyBottom =
        tft.color565(255, 115, 180);

      theme.celestial =
        tft.color565(255, 245, 180);

      theme.cloud =
        tft.color565(255, 215, 235);

      theme.mountainFar =
        tft.color565(140, 105, 185);

      theme.mountainMid =
        tft.color565(100, 70, 145);

      theme.mountainNear =
        tft.color565(70, 40, 110);

      theme.ground =
        tft.color565(55, 80, 65);

      theme.tree =
        tft.color565(25, 55, 40);

      theme.snow =
        tft.color565(255, 245, 250);

      theme.water =
        tft.color565(95, 75, 145);

      theme.reflection =
        tft.color565(240, 150, 210);

      theme.text =
        TFT_WHITE;

      theme.panel =
        TFT_BLACK;

      theme.night = false;
      break;

    // Alien teal
    case 3:
      theme.skyTop =
        tft.color565(0, 145, 165);

      theme.skyBottom =
        tft.color565(140, 250, 195);

      theme.celestial =
        tft.color565(245, 255, 115);

      theme.cloud =
        tft.color565(220, 255, 240);

      theme.mountainFar =
        tft.color565(100, 175, 155);

      theme.mountainMid =
        tft.color565(70, 135, 115);

      theme.mountainNear =
        tft.color565(45, 95, 80);

      theme.ground =
        tft.color565(35, 105, 75);

      theme.tree =
        tft.color565(15, 70, 45);

      theme.snow =
        tft.color565(220, 255, 235);

      theme.water =
        tft.color565(10, 115, 125);

      theme.reflection =
        tft.color565(170, 255, 210);

      theme.text =
        TFT_WHITE;

      theme.panel =
        TFT_BLACK;

      theme.night = false;
      break;

    // Blue night
    case 4:
      theme.skyTop =
        tft.color565(5, 8, 35);

      theme.skyBottom =
        tft.color565(35, 65, 130);

      theme.celestial =
        tft.color565(245, 245, 215);

      theme.cloud =
        tft.color565(135, 155, 190);

      theme.mountainFar =
        tft.color565(50, 75, 125);

      theme.mountainMid =
        tft.color565(30, 50, 90);

      theme.mountainNear =
        tft.color565(18, 30, 65);

      theme.ground =
        tft.color565(15, 40, 35);

      theme.tree =
        tft.color565(8, 22, 20);

      theme.snow =
        tft.color565(215, 225, 240);

      theme.water =
        tft.color565(12, 35, 70);

      theme.reflection =
        tft.color565(100, 130, 175);

      theme.text =
        TFT_WHITE;

      theme.panel =
        TFT_BLACK;

      theme.night = true;
      break;

    // Purple night
    default:
      theme.skyTop =
        tft.color565(15, 0, 40);

      theme.skyBottom =
        tft.color565(95, 15, 115);

      theme.celestial =
        tft.color565(255, 235, 150);

      theme.cloud =
        tft.color565(160, 120, 200);

      theme.mountainFar =
        tft.color565(90, 60, 135);

      theme.mountainMid =
        tft.color565(60, 35, 100);

      theme.mountainNear =
        tft.color565(35, 18, 75);

      theme.ground =
        tft.color565(20, 45, 45);

      theme.tree =
        tft.color565(8, 25, 25);

      theme.snow =
        tft.color565(230, 220, 245);

      theme.water =
        tft.color565(45, 20, 80);

      theme.reflection =
        tft.color565(185, 95, 200);

      theme.text =
        TFT_WHITE;

      theme.panel =
        TFT_BLACK;

      theme.night = true;
      break;
  }
}

// ======================================================
// Sky
// ======================================================

void drawGradientSky() {

  for (int y = 0; y < SCREEN_H; y++) {

    float t =
      (float)y / (SCREEN_H - 1);

    uint16_t color =
      blend565(
        theme.skyTop,
        theme.skyBottom,
        t
      );

    tft.drawFastHLine(
      0,
      y,
      SCREEN_W,
      color
    );
  }
}

// ======================================================
// Stars
// ======================================================

void drawStars() {

  int count = randRange(25, 55);

  for (int i = 0; i < count; i++) {

    int x = randRange(2, SCREEN_W - 3);
    int y = randRange(20, 110);

    uint16_t c =
      chance(80)
      ? theme.text
      : theme.celestial;

    tft.drawPixel(x, y, c);

    if (chance(12)) {
      tft.drawPixel(x + 1, y, c);
      tft.drawPixel(x - 1, y, c);
    }
  }
}

// ======================================================
// Shooting star
// ======================================================

void drawShootingStar() {

  if (!theme.night || !chance(18)) {
    return;
  }

  int x = randRange(80, 280);
  int y = randRange(30, 80);

  for (int i = 0; i < 25; i++) {

    uint16_t c =
      blend565(
        theme.celestial,
        theme.skyTop,
        (float)i / 25
      );

    tft.drawPixel(
      x - i,
      y + i / 3,
      c
    );
  }

  tft.fillCircle(
    x,
    y,
    2,
    theme.celestial
  );
}

// ======================================================
// Clouds
// ======================================================

void drawCloud(
  int cx,
  int cy,
  int scale,
  uint16_t color
) {

  tft.fillCircle(
    cx - scale,
    cy,
    scale,
    color
  );

  tft.fillCircle(
    cx,
    cy - scale / 3,
    scale + 2,
    color
  );

  tft.fillCircle(
    cx + scale,
    cy,
    scale,
    color
  );

  tft.fillRect(
    cx - scale * 2,
    cy,
    scale * 4,
    scale,
    color
  );
}

void drawClouds() {

  int count = randRange(1, 4);

  for (int i = 0; i < count; i++) {

    int x =
      randRange(35, SCREEN_W - 35);

    int y =
      randRange(30, 85);

    int s =
      randRange(8, 15);

    drawCloud(
      x,
      y,
      s,
      theme.cloud
    );
  }
}

// ======================================================
// Sun / Moon
// ======================================================

void drawCelestial() {

  celestialX =
    randRange(45, SCREEN_W - 45);

  celestialY =
    randRange(35, 75);

  celestialR =
    randRange(15, 23);

  tft.fillCircle(
    celestialX,
    celestialY,
    celestialR,
    theme.celestial
  );

  if (theme.night && chance(65)) {

    uint16_t cut =
      blend565(
        theme.skyTop,
        theme.skyBottom,
        (float)celestialY /
        SCREEN_H
      );

    tft.fillCircle(
      celestialX +
      celestialR / 3,
      celestialY -
      celestialR / 5,
      celestialR - 3,
      cut
    );
  }
}

// ======================================================
// Better ridge generation
// ======================================================

void generateMountainRidge(
  Ridge &ridge,
  int baseY,
  int peakMin,
  int peakMax,
  int segmentMin,
  int segmentMax
) {

  ridge.count = 0;

  int x = 0;

  ridge.xs[ridge.count] = 0;
  ridge.ys[ridge.count] = baseY;
  ridge.count++;

  while (
    x < SCREEN_W &&
    ridge.count < MAX_POINTS - 2
  ) {

    int width =
      randRange(
        segmentMin,
        segmentMax
      );

    int peakX =
      x + width / 2;

    int endX =
      x + width;

    if (endX > SCREEN_W) {
      endX = SCREEN_W;
    }

    int peakY =
      randRange(
        peakMin,
        peakMax
      );

    ridge.xs[ridge.count] =
      min(peakX, SCREEN_W);

    ridge.ys[ridge.count] =
      peakY;

    ridge.count++;

    ridge.xs[ridge.count] =
      endX;

    ridge.ys[ridge.count] =
      baseY + randRange(-5, 8);

    ridge.count++;

    x = endX;
  }
}

// ======================================================
// Ridge drawing
// ======================================================

void drawRidgeFilled(
  const Ridge &ridge,
  uint16_t color
) {

  for (
    int i = 0;
    i < ridge.count - 1;
    i++
  ) {

    int x1 = ridge.xs[i];
    int y1 = ridge.ys[i];

    int x2 = ridge.xs[i + 1];
    int y2 = ridge.ys[i + 1];

    if (x2 <= x1) {
      continue;
    }

    for (
      int x = x1;
      x <= x2;
      x++
    ) {

      float t =
        (float)(x - x1) /
        (float)(x2 - x1);

      int y =
        y1 +
        (y2 - y1) * t;

      tft.drawFastVLine(
        x,
        y,
        SCREEN_H - y,
        color
      );
    }
  }
}

// ======================================================
// Snow
// ======================================================

void drawSnowCaps(
  const Ridge &ridge
) {

  for (
    int i = 1;
    i < ridge.count - 1;
    i++
  ) {

    // Peaks tend to be odd entries

    if (ridge.ys[i] < ridge.ys[i - 1] &&
        ridge.ys[i] < ridge.ys[i + 1]) {

      if (!chance(70)) {
        continue;
      }

      int x =
        ridge.xs[i];

      int y =
        ridge.ys[i];

      int w =
        randRange(10, 18);

      int h =
        randRange(8, 14);

      tft.fillTriangle(
        x - w,
        y + h,
        x,
        y,
        x + w,
        y + h,
        theme.snow
      );
    }
  }
}

// ======================================================
// Terrain lookup
// ======================================================

int ridgeYAt(
  const Ridge &ridge,
  int x
) {

  for (
    int i = 0;
    i < ridge.count - 1;
    i++
  ) {

    int x1 =
      ridge.xs[i];

    int x2 =
      ridge.xs[i + 1];

    if (
      x >= x1 &&
      x <= x2
    ) {

      int y1 =
        ridge.ys[i];

      int y2 =
        ridge.ys[i + 1];

      if (x2 == x1) {
        return y1;
      }

      float t =
        (float)(x - x1) /
        (float)(x2 - x1);

      return
        y1 +
        (y2 - y1) * t;
    }
  }

  return SCREEN_H - 20;
}

// ======================================================
// Lake
// ======================================================

void drawLake() {

  const int waterTop = 168;

  tft.fillRect(
    0,
    waterTop,
    SCREEN_W,
    SCREEN_H - waterTop,
    theme.water
  );

  // Horizontal water texture

  for (int i = 0; i < 30; i++) {

    int y =
      randRange(
        waterTop + 3,
        SCREEN_H - 5
      );

    int x =
      randRange(0, SCREEN_W - 25);

    int len =
      randRange(8, 35);

    uint16_t c =
      chance(50)
      ? theme.reflection
      : blend565(
          theme.water,
          theme.skyBottom,
          0.35
        );

    tft.drawFastHLine(
      x,
      y,
      len,
      c
    );
  }

  // Sun/moon reflection

  int reflectionStart =
    waterTop + 4;

  for (
    int y = reflectionStart;
    y < SCREEN_H - 8;
    y += 5
  ) {

    int distance =
      y - reflectionStart;

    int width =
      6 + distance / 3;

    int jitter =
      randRange(-7, 7);

    int x =
      celestialX -
      width / 2 +
      jitter;

    x =
      constrain(
        x,
        0,
        SCREEN_W - width
      );

    tft.drawFastHLine(
      x,
      y,
      width,
      theme.reflection
    );
  }
}

// ======================================================
// Trees
// ======================================================

void drawPineTree(
  int x,
  int groundY,
  int scale,
  uint16_t color
) {

  int trunkH =
    max(4, scale / 3);

  int trunkW =
    max(2, scale / 6);

  uint16_t trunk =
    tft.color565(80, 45, 25);

  tft.fillRect(
    x - trunkW / 2,
    groundY - trunkH,
    trunkW,
    trunkH,
    trunk
  );

  tft.fillTriangle(
    x - scale / 2,
    groundY - trunkH,
    x,
    groundY - scale,
    x + scale / 2,
    groundY - trunkH,
    color
  );

  tft.fillTriangle(
    x - scale / 2 + 3,
    groundY - scale / 3,
    x,
    groundY - scale - 8,
    x + scale / 2 - 3,
    groundY - scale / 3,
    color
  );
}

// ======================================================
// Random trees
// ======================================================

void drawTrees() {

  int count =
    randRange(6, 12);

  for (int i = 0; i < count; i++) {

    int x =
      randRange(
        10,
        SCREEN_W - 10
      );

    int y =
      ridgeYAt(
        foregroundRidge,
        x
      );

    int size =
      randRange(15, 28);

    drawPineTree(
      x,
      y,
      size,
      theme.tree
    );
  }
}

// ======================================================
// Foreground framing trees
// ======================================================

void drawForegroundTrees() {

  if (chance(65)) {

    drawPineTree(
      18,
      225,
      randRange(38, 52),
      theme.tree
    );
  }

  if (chance(65)) {

    drawPineTree(
      300,
      225,
      randRange(38, 52),
      theme.tree
    );
  }
}

// ======================================================
// Birds
// ======================================================

void drawBirds() {

  if (theme.night ||
      !chance(20)) {
    return;
  }

  int bx =
    randRange(60, 250);

  int by =
    randRange(45, 95);

  for (int i = 0; i < 3; i++) {

    int x =
      bx + i * 12;

    int y =
      by + randRange(-5, 5);

    tft.drawLine(
      x - 4,
      y + 2,
      x,
      y,
      theme.mountainNear
    );

    tft.drawLine(
      x,
      y,
      x + 4,
      y + 2,
      theme.mountainNear
    );
  }
}

// ======================================================
// Footer
// ======================================================

void drawFooter() {

  tft.fillRect(
    0,
    222,
    SCREEN_W,
    18,
    theme.panel
  );

  tft.setTextColor(
    theme.text,
    theme.panel
  );

  tft.setTextDatum(TL_DATUM);

  tft.drawString(
    "WORLD #" +
    String(worldSeed),
    5,
    227,
    1
  );

  tft.setTextDatum(TR_DATUM);

  tft.drawString(
    "BOOT: NEW WORLD",
    315,
    227,
    1
  );
}

// ======================================================
// Generate landscape
// ======================================================

void generateWorld(
  bool newSeed = true
) {

  if (newSeed) {
    worldSeed =
      esp_random();
  }

  randomSeed(worldSeed);

  chooseTheme();

  hasLake =
    chance(42);

  // ------------------------------------
  // Stage 1: Sky
  // ------------------------------------

  drawGradientSky();

  // ------------------------------------
  // Stage 2: Atmosphere
  // ------------------------------------

  if (theme.night) {

    drawStars();
    drawShootingStar();

  } else {

    if (chance(75)) {
      drawClouds();
    }
  }

  drawCelestial();

  drawBirds();

  // ------------------------------------
  // Stage 3: Generate mountains
  // ------------------------------------

  generateMountainRidge(
    farRidge,
    135,
    72,
    108,
    60,
    100
  );

  generateMountainRidge(
    midRidge,
    160,
    95,
    125,
    50,
    80
  );

  generateMountainRidge(
    nearRidge,
    188,
    118,
    150,
    45,
    75
  );

  generateMountainRidge(
    foregroundRidge,
    214,
    165,
    195,
    40,
    70
  );

  // ------------------------------------
  // Stage 4: Paint mountains
  // ------------------------------------

  drawRidgeFilled(
    farRidge,
    theme.mountainFar
  );

  drawSnowCaps(
    farRidge
  );

  drawRidgeFilled(
    midRidge,
    theme.mountainMid
  );

  if (chance(50)) {
    drawSnowCaps(
      midRidge
    );
  }

  drawRidgeFilled(
    nearRidge,
    theme.mountainNear
  );

  // ------------------------------------
  // Stage 5: Lake OR ground
  // ------------------------------------

  if (hasLake) {

    drawLake();

    // redraw nearby mountain edge
    drawRidgeFilled(
      nearRidge,
      theme.mountainNear
    );
  }

  drawRidgeFilled(
    foregroundRidge,
    theme.ground
  );

  // ------------------------------------
  // Stage 6: Vegetation
  // ------------------------------------

  drawTrees();
  drawForegroundTrees();

  // ------------------------------------
  // Stage 7: UI
  // ------------------------------------

  drawFooter();
}

// ======================================================
// Button
// ======================================================

void updateButton() {

  bool reading =
    digitalRead(
      BOOT_BUTTON
    );

  if (
    reading !=
    lastButtonReading
  ) {

    lastDebounceTime =
      millis();

    lastButtonReading =
      reading;
  }

  if (
    millis() -
    lastDebounceTime >
    DEBOUNCE_MS
  ) {

    if (
      reading !=
      stableButtonState
    ) {

      stableButtonState =
        reading;

      // Press
      if (
        stableButtonState ==
        LOW
      ) {

        buttonDownTime =
          millis();
      }

      // Release
      else {

        unsigned long held =
          millis() -
          buttonDownTime;

        if (
          held >= HOLD_MS
        ) {

          // Repaint same deterministic world
          generateWorld(false);

        } else {

          // Generate new world
          generateWorld(true);
        }
      }
    }
  }
}

// ======================================================
// Setup
// ======================================================

void setup() {

  Serial.begin(115200);

  pinMode(
    TFT_BACKLIGHT,
    OUTPUT
  );

  digitalWrite(
    TFT_BACKLIGHT,
    HIGH
  );

  pinMode(
    BOOT_BUTTON,
    INPUT_PULLUP
  );

  tft.init();

  tft.invertDisplay(true);

  tft.setRotation(1);

  worldSeed =
    esp_random();

  generateWorld(false);
}

// ======================================================
// Main loop
// ======================================================

void loop() {

  updateButton();
}