
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

// ============================================================
// CYD SOLAR SYSTEM
// ESP32-2432S028R / ILI9341 / 320x240 landscape
//
// Features:
//   - Animated Sun and eight planets
//   - Black background and starfield
//   - Smooth sprite-based rendering
//   - Saturn's rings and Jupiter's stripes
//   - Automatic planet information display
//   - No touchscreen or Wi-Fi required
//
// IMPORTANT:
// This CYD requires tft.invertDisplay(true).
// ============================================================

// -------------------- Display --------------------

TFT_eSPI tft = TFT_eSPI();

// Orbit animation area.
// 220 x 190 x 2 bytes = approximately 84 KB of RAM.
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

constexpr int SPACE_X = 0;
constexpr int SPACE_Y = 25;
constexpr int SPACE_W = 220;
constexpr int SPACE_H = 190;

constexpr int SUN_X = 110;
constexpr int SUN_Y = 95;

TFT_eSprite spaceSprite = TFT_eSprite(&tft);
bool spriteReady = false;

// -------------------- Colors --------------------

constexpr uint16_t BLACK  = TFT_BLACK;
constexpr uint16_t WHITE  = TFT_WHITE;
constexpr uint16_t CYAN   = TFT_CYAN;
constexpr uint16_t YELLOW = TFT_YELLOW;
constexpr uint16_t GRAY   = 0x8410;
constexpr uint16_t DIM    = 0x4208;
constexpr uint16_t ORBIT  = 0x2945;

// -------------------- Planets --------------------

struct Planet {
  const char* name;
  uint16_t color;
  float orbitRadius;
  float period;
  uint8_t radius;
  float diameter;
  float distanceAU;
};

Planet planets[] = {
  {"MERCURY", 0xAD55,  20,    0.241, 2,   4879,  0.39},
  {"VENUS",   0xF5A6,  30,    0.615, 3,  12104,  0.72},
  {"EARTH",   0x05FF,  41,    1.000, 4,  12742,  1.00},
  {"MARS",    0xF9A4,  52,    1.881, 3,   6779,  1.52},
  {"JUPITER", 0xFD20,  64,   11.862, 7, 139820,  5.20},
  {"SATURN",  0xEED0,  75,   29.457, 6, 116460,  9.58},
  {"URANUS",  0x7EFF,  85,   84.017, 4,  50724, 19.20},
  {"NEPTUNE", 0x421F,  92,  164.790, 4,  49244, 30.05}
};

constexpr int PLANET_COUNT =
    sizeof(planets) / sizeof(planets[0]);

// -------------------- Animation --------------------

float simulationYears = 0.0f;

// Simulated years per real second.
// Exaggerated relative motion keeps outer planets interesting.
float animationSpeed = 0.12f;

unsigned long lastFrame = 0;
unsigned long lastSelection = 0;

constexpr unsigned long FRAME_INTERVAL = 50; // ~20 FPS
constexpr unsigned long SELECTION_INTERVAL = 5000;

int selectedPlanet = 2; // Earth

// -------------------- Stars --------------------

struct Star {
  uint8_t x;
  uint8_t y;
  uint16_t color;
};

constexpr int STAR_COUNT = 65;
Star stars[STAR_COUNT];

// ============================================================
// Drawing helpers
// ============================================================

void drawCenteredText(
    TFT_eSprite& sprite,
    const char* text,
    int x,
    int y,
    uint16_t color,
    int font = 2) {

  sprite.setTextDatum(MC_DATUM);
  sprite.setTextColor(color, BLACK);
  sprite.drawString(text, x, y, font);
}

void initializeStars() {
  randomSeed(micros());

  for (int i = 0; i < STAR_COUNT; i++) {
    stars[i].x = random(SPACE_W);
    stars[i].y = random(SPACE_H);

    int brightness = random(3);

    if (brightness == 0)
      stars[i].color = DIM;
    else if (brightness == 1)
      stars[i].color = GRAY;
    else
      stars[i].color = WHITE;
  }
}

void drawStars() {
  for (int i = 0; i < STAR_COUNT; i++) {
    spaceSprite.drawPixel(
        stars[i].x,
        stars[i].y,
        stars[i].color);
  }
}

// ============================================================
// Sun
// ============================================================

void drawSun() {
  // Outer glow
  spaceSprite.fillCircle(SUN_X, SUN_Y, 11, 0x7800);
  spaceSprite.fillCircle(SUN_X, SUN_Y, 9, 0xFA00);

  // Main solar disk
  spaceSprite.fillCircle(SUN_X, SUN_Y, 7, YELLOW);
  spaceSprite.fillCircle(SUN_X - 2, SUN_Y - 2, 3, WHITE);

  // Rays
  for (int i = 0; i < 8; i++) {
    float a = i * TWO_PI / 8.0f;

    int x1 = SUN_X + cosf(a) * 13;
    int y1 = SUN_Y + sinf(a) * 13;

    int x2 = SUN_X + cosf(a) * 16;
    int y2 = SUN_Y + sinf(a) * 16;

    spaceSprite.drawLine(x1, y1, x2, y2, 0xFD20);
  }
}

// ============================================================
// Orbit paths
// ============================================================

void drawOrbitPaths() {
  for (int i = 0; i < PLANET_COUNT; i++) {
    int r = (int)planets[i].orbitRadius;

    uint16_t color =
        (i == selectedPlanet) ? 0x4A49 : ORBIT;

    spaceSprite.drawCircle(SUN_X, SUN_Y, r, color);
  }
}

// ============================================================
// Planet positions
// ============================================================

void getPlanetPosition(int index, int& x, int& y) {
  const Planet& p = planets[index];

  // Compressed orbital periods improve visible motion.
  // This is intentionally not a physical simulation.
  float visualPeriod = sqrtf(p.period);

  float angle =
      simulationYears * TWO_PI / visualPeriod
      + index * 0.75f;

  x = SUN_X + cosf(angle) * p.orbitRadius;
  y = SUN_Y + sinf(angle) * p.orbitRadius;
}

// ============================================================
// Planet graphics
// ============================================================

void drawPlanet(int index) {
  Planet& p = planets[index];

  int x, y;
  getPlanetPosition(index, x, y);

  // Selected planet indicator
  if (index == selectedPlanet) {
    spaceSprite.drawCircle(
        x, y, p.radius + 4, CYAN);
  }

  // Saturn's rings behind the planet
  if (index == 5) {
    spaceSprite.drawEllipse(
        x, y, 12, 4, 0xCDB0);
    spaceSprite.drawEllipse(
        x, y, 10, 3, 0x8C71);
  }

  // Planet body
  spaceSprite.fillCircle(
      x, y, p.radius, p.color);

  // Small lighting highlight
  if (p.radius >= 3) {
    spaceSprite.fillCircle(
        x - 1, y - 1, 1, WHITE);
  }

  // Earth: simple green landmass
  if (index == 2) {
    spaceSprite.fillCircle(
        x + 1, y + 1, 2, TFT_GREEN);
  }

  // Jupiter: cloud bands
  if (index == 4) {
    spaceSprite.drawFastHLine(
        x - 5, y - 2, 11, 0xA145);
    spaceSprite.drawFastHLine(
        x - 6, y + 2, 13, 0xA145);
  }

  // Saturn: front ring
  if (index == 5) {
    spaceSprite.drawArc(
        x, y, 12, 10, 20, 160,
        0xEED0, BLACK);
  }
}

// ============================================================
// Space rendering
// ============================================================

void renderSpace() {
  spaceSprite.fillSprite(BLACK);

  drawStars();
  drawOrbitPaths();
  drawSun();

  for (int i = 0; i < PLANET_COUNT; i++) {
    drawPlanet(i);
  }

  // Transfer the completed frame in one operation.
  spaceSprite.pushSprite(SPACE_X, SPACE_Y);
}

// ============================================================
// Static interface
// ============================================================

void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, 25, BLACK);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(CYAN, BLACK);

  tft.drawString(
      "CYD SOLAR SYSTEM", 8, 5, 2);

  tft.drawFastHLine(
      0, 24, SCREEN_W, DIM);
}

void drawFooter() {
  tft.fillRect(0, 215, SCREEN_W, 25, BLACK);

  tft.drawFastHLine(
      0, 215, SCREEN_W, DIM);

  tft.setTextColor(GRAY, BLACK);
  tft.setTextDatum(TL_DATUM);

  tft.drawString(
      "AUTO TOUR", 8, 221, 2);

  tft.setTextDatum(TR_DATUM);
  tft.drawString(
      "8 PLANETS", 312, 221, 2);
}

// ============================================================
// Planet information panel
// ============================================================

void drawInfoPanel() {
  const Planet& p = planets[selectedPlanet];

  constexpr int X = 225;
  constexpr int Y = 29;
  constexpr int W = 95;
  constexpr int H = 183;

  tft.fillRect(X, Y, W, H, BLACK);

  tft.drawRoundRect(
      X, Y, W - 3, H, 5, DIM);

  tft.setTextDatum(TC_DATUM);

  tft.setTextColor(CYAN, BLACK);
  tft.drawString(
      p.name, X + 46, Y + 8, 2);

  // Decorative planet preview
  tft.fillCircle(
      X + 46, Y + 52, 19, p.color);

  tft.fillCircle(
      X + 39, Y + 45, 5, WHITE);

  if (selectedPlanet == 5) {
    tft.drawEllipse(
        X + 46, Y + 52,
        29, 9, 0xCDB0);
  }

  tft.setTextDatum(TL_DATUM);

  tft.setTextColor(GRAY, BLACK);
  tft.drawString(
      "DIAMETER", X + 7, Y + 83, 1);

  tft.setTextColor(WHITE, BLACK);

  char buffer[32];

  snprintf(
      buffer, sizeof(buffer),
      "%.0f km", p.diameter);

  tft.drawString(
      buffer, X + 7, Y + 95, 2);

  tft.setTextColor(GRAY, BLACK);
  tft.drawString(
      "DISTANCE", X + 7, Y + 118, 1);

  snprintf(
      buffer, sizeof(buffer),
      "%.2f AU", p.distanceAU);

  tft.setTextColor(WHITE, BLACK);
  tft.drawString(
      buffer, X + 7, Y + 130, 2);

  tft.setTextColor(GRAY, BLACK);
  tft.drawString(
      "ORBITAL PERIOD", X + 7, Y + 153, 1);

  snprintf(
      buffer, sizeof(buffer),
      "%.2f yr", p.period);

  tft.setTextColor(WHITE, BLACK);
  tft.drawString(
      buffer, X + 7, Y + 164, 2);
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);

  // REQUIRED for this particular CYD display.
  // Without inversion, black appears white.
  tft.invertDisplay(true);

  // Turn on backlight
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.fillScreen(TFT_BLACK);

  initializeStars();

  // Create 16-bit off-screen sprite
  spaceSprite.setColorDepth(16);

  void* buffer =
      spaceSprite.createSprite(SPACE_W, SPACE_H);

  if (buffer == nullptr) {
    Serial.println(
        "ERROR: Could not allocate space sprite");

    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString(
        "SPRITE ALLOCATION FAILED", 10, 100, 2);

    while (true) {
      delay(1000);
    }
  }

  spriteReady = true;

  drawHeader();
  drawFooter();
  drawInfoPanel();

  lastFrame = millis();
  lastSelection = millis();

  Serial.println("CYD Solar System started");
}

// ============================================================
// Main loop
// ============================================================

void loop() {
  unsigned long now = millis();

  // Automatically cycle through planets
  if (now - lastSelection >= SELECTION_INTERVAL) {
    selectedPlanet =
        (selectedPlanet + 1) % PLANET_COUNT;

    lastSelection = now;

    drawInfoPanel();
  }

  // Animation timing
  if (now - lastFrame >= FRAME_INTERVAL) {
    float deltaSeconds =
        (now - lastFrame) / 1000.0f;

    lastFrame = now;

    simulationYears +=
        deltaSeconds * animationSpeed;

    if (simulationYears > 10000.0f) {
      simulationYears = 0.0f;
    }

    if (spriteReady) {
      renderSpace();
    }
  }
}
