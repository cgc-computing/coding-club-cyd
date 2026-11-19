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
TFT_eSprite spr = TFT_eSprite(&tft);

// ---------- Screen / scene ----------
#define SCR_W      240
#define SCR_H      320
#define SPR_H      250
#define FLOOR_Y    215.0f
#define LEFT_WALL  0.0f
#define RIGHT_WALL 240.0f
#define NUM_DICE   3
#define DICE_R     26.0f          // smaller so 3 fit comfortably

// ---------- Icosahedron geometry ----------
const float PHI = 1.618034f;

const float baseVerts[12][3] = {
  {-1,  PHI, 0}, { 1,  PHI, 0}, {-1, -PHI, 0}, { 1, -PHI, 0},
  { 0, -1,  PHI}, { 0,  1,  PHI}, { 0, -1, -PHI}, { 0,  1, -PHI},
  { PHI, 0, -1}, { PHI, 0,  1}, {-PHI, 0, -1}, {-PHI, 0,  1}
};

const uint8_t faces[20][3] = {
  {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
  {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
  {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
  {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
};

// ---------- Physics constants ----------
const float GRAVITY       = 0.55f;
const float BOUNCE        = 0.60f;
const float WALL_BOUNCE   = 0.70f;
const float AIR_DRAG      = 0.995f;
const float ROLL_FRICTION = 0.96f;
const float SPIN_DAMP     = 0.985f;

// ---------- Light ----------
const float lightDir[3] = {0.4f, -0.6f, 0.7f};

// ---------- Dice struct ----------
struct Die {
  float x, y;                 // center position
  float vx, vy;
  float ax, ay, az;           // rotation angles
  float avx, avy, avz;        // angular velocity
  int   result;
  int   shown;                // number currently displayed
  bool  settled;
  float baseR, baseG, baseB;  // color
  float verts[12][3];
  float px[12], py[12];
  unsigned long lastNum;
};

Die dice[NUM_DICE];

// ---------- Game state ----------
enum State { IDLE, ROLLING, SETTLED };
State state = IDLE;
bool wasTouched = false;
int  history[5] = {0, 0, 0, 0, 0};

// ---------- Dice setup ----------
void initDice() {
  float cols[NUM_DICE][3] = {
    {235, 50, 60},    // red
    {50, 110, 240},   // blue
    {50, 200, 90}     // green
  };
  for (int i = 0; i < NUM_DICE; i++) {
    Die &d = dice[i];
    d.x = 50 + i * 70;
    d.y = FLOOR_Y - DICE_R;
    d.vx = d.vy = 0;
    d.ax = 0.4f + i; d.ay = 0.6f + i * 0.7f; d.az = 0;
    d.avx = d.avy = d.avz = 0;
    d.result = 0;
    d.shown = 100;
    d.settled = true;
    d.baseR = cols[i][0]; d.baseG = cols[i][1]; d.baseB = cols[i][2];
    d.lastNum = 0;
  }
}

void rotateVerts(Die &d) {
  float cx = cosf(d.ax), sx = sinf(d.ax);
  float cy = cosf(d.ay), sy = sinf(d.ay);
  float cz = cosf(d.az), sz = sinf(d.az);

  for (int i = 0; i < 12; i++) {
    float x = baseVerts[i][0];
    float y = baseVerts[i][1];
    float z = baseVerts[i][2];

    float y1 = y * cx - z * sx;
    float z1 = y * sx + z * cx;
    float x2 = x * cy + z1 * sy;
    float z2 = -x * sy + z1 * cy;
    float x3 = x2 * cz - y1 * sz;
    float y3 = x2 * sz + y1 * cz;

    d.verts[i][0] = x3;
    d.verts[i][1] = y3;
    d.verts[i][2] = z2;
  }
}

void projectVerts(Die &d) {
  float scale = DICE_R / 1.9f;
  float camDist = 7.0f;
  for (int i = 0; i < 12; i++) {
    float z = d.verts[i][2] + camDist;
    float f = camDist / z;
    d.px[i] = d.x + d.verts[i][0] * scale * f;
    d.py[i] = d.y + d.verts[i][1] * scale * f;
  }
}

uint16_t shadeColor(const Die &d, float b) {
  b = constrain(b, 0.15f, 1.0f);
  return tft.color565((uint8_t)(d.baseR * b),
                      (uint8_t)(d.baseG * b),
                      (uint8_t)(d.baseB * b));
}

struct FaceDraw { float depth; uint8_t idx; };

void drawDie(Die &d) {
  rotateVerts(d);
  projectVerts(d);

  FaceDraw order[20];
  int n = 0;

  for (int f = 0; f < 20; f++) {
    float *a = d.verts[faces[f][0]];
    float *b = d.verts[faces[f][1]];
    float *c = d.verts[faces[f][2]];

    float ux = b[0]-a[0], uy = b[1]-a[1];
    float vx = c[0]-a[0], vy = c[1]-a[1];
    float nz = ux * vy - uy * vx;
    if (nz >= 0) continue;

    order[n].depth = (a[2] + b[2] + c[2]) / 3.0f;
    order[n].idx = f;
    n++;
  }

  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (order[j].depth > order[i].depth) {
        FaceDraw t = order[i]; order[i] = order[j]; order[j] = t;
      }

  for (int i = 0; i < n; i++) {
    int f = order[i].idx;
    float *a = d.verts[faces[f][0]];
    float *b = d.verts[faces[f][1]];
    float *c = d.verts[faces[f][2]];

    float ux = b[0]-a[0], uy = b[1]-a[1], uz = b[2]-a[2];
    float vx = c[0]-a[0], vy = c[1]-a[1], vz = c[2]-a[2];
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float len = sqrtf(nx*nx + ny*ny + nz*nz);
    if (len < 0.0001f) continue;
    nx /= len; ny /= len; nz /= len;

    float bright = -(nx * lightDir[0] + ny * lightDir[1] + nz * lightDir[2]);
    bright = 0.35f + 0.65f * constrain(bright, 0.0f, 1.0f);

    uint16_t col = shadeColor(d, bright);
    int i0 = faces[f][0], i1 = faces[f][1], i2 = faces[f][2];

    spr.fillTriangle((int)d.px[i0], (int)d.py[i0],
                     (int)d.px[i1], (int)d.py[i1],
                     (int)d.px[i2], (int)d.py[i2], col);
    spr.drawTriangle((int)d.px[i0], (int)d.py[i0],
                     (int)d.px[i1], (int)d.py[i1],
                     (int)d.px[i2], (int)d.py[i2], 0xFFDF);
  }
}

// ---------- Scene drawing ----------
void drawShadow(const Die &d) {
  float height = FLOOR_Y - DICE_R - d.y;
  if (height < 0) height = 0;
  float sh = constrain(1.0f - height / 160.0f, 0.15f, 1.0f);
  int shW = (int)(DICE_R * (0.5f + 0.9f * sh));
  int shH = (int)(4 + 4 * sh);
  spr.fillEllipse((int)d.x, (int)FLOOR_Y + 8, shW, shH, 0x0120);
}

void drawScene() {
  spr.fillSprite(0x0000);

  // table
  spr.fillRect(0, (int)FLOOR_Y, SCR_W, SPR_H - (int)FLOOR_Y, 0x0320);
  spr.drawFastHLine(0, (int)FLOOR_Y, SCR_W, 0x07E0);

  // shadows first
  for (int i = 0; i < NUM_DICE; i++) drawShadow(dice[i]);

  // draw dice back-to-front by vertical position (lower on screen = closer)
  int order[NUM_DICE];
  for (int i = 0; i < NUM_DICE; i++) order[i] = i;
  for (int i = 0; i < NUM_DICE - 1; i++)
    for (int j = i + 1; j < NUM_DICE; j++)
      if (dice[order[j]].y < dice[order[i]].y) {
        int t = order[i]; order[i] = order[j]; order[j] = t;
      }

  for (int i = 0; i < NUM_DICE; i++) drawDie(dice[order[i]]);

  // numbers above each die
  spr.setTextDatum(MC_DATUM);
  for (int i = 0; i < NUM_DICE; i++) {
    Die &d = dice[i];
    int ty = (int)d.y - (int)DICE_R - 14;
    if (ty < 12) ty = 12;
    spr.setTextColor(TFT_BLACK);
    spr.drawNumber(d.shown, (int)d.x + 1, ty + 1, 2);
    spr.setTextColor(state == SETTLED ? TFT_YELLOW : TFT_WHITE);
    spr.drawNumber(d.shown, (int)d.x, ty, 2);
  }
  spr.setTextDatum(TL_DATUM);

  spr.pushSprite(0, 0);
}

void drawUI() {
  tft.fillRect(0, SPR_H, SCR_W, SCR_H - SPR_H, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);

  if (state == ROLLING) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Rolling...", SCR_W / 2, SPR_H + 12, 2);
  } else if (state == SETTLED) {
    int total = 0;
    for (int i = 0; i < NUM_DICE; i++) total += dice[i].result;
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(dice[0].result) + " + " + String(dice[1].result) +
                   " + " + String(dice[2].result) + " = " + String(total),
                   SCR_W / 2, SPR_H + 12, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Tap to roll again", SCR_W / 2, SPR_H + 30, 2);
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Tap to roll 3 x d100", SCR_W / 2, SPR_H + 12, 2);
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Previous totals", SCR_W / 2, SPR_H + 48, 2);

  String h = "";
  for (int i = 0; i < 5; i++)
    if (history[i] > 0) h += String(history[i]) + "  ";
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(h.length() ? h : "-", SCR_W / 2, SPR_H + 66, 2);
  tft.setTextDatum(TL_DATUM);
}

// ---------- Roll logic ----------
void startRoll() {
  state = ROLLING;

  for (int i = 0; i < NUM_DICE; i++) {
    Die &d = dice[i];
    d.result = random(1, 101);
    d.settled = false;

    bool fromLeft = random(0, 2);
    d.x = fromLeft ? 25 + i * 15 : 215 - i * 15;
    d.y = 20 + i * 25;
    d.vx = (fromLeft ? 1 : -1) * (3.0f + random(0, 35) / 10.0f);
    d.vy = -2.0f + random(0, 25) / 10.0f;

    d.avx = (random(0, 2) ? 1 : -1) * (0.25f + random(0, 20) / 100.0f);
    d.avy = (random(0, 2) ? 1 : -1) * (0.30f + random(0, 20) / 100.0f);
    d.avz = (random(0, 2) ? 1 : -1) * (0.15f + random(0, 15) / 100.0f);
  }
  drawUI();
}

void finishRoll() {
  state = SETTLED;
  int total = 0;
  for (int i = 0; i < NUM_DICE; i++) {
    dice[i].shown = dice[i].result;
    dice[i].vx = dice[i].vy = 0;
    dice[i].avx = dice[i].avy = dice[i].avz = 0;
    total += dice[i].result;
  }
  for (int i = 4; i > 0; i--) history[i] = history[i - 1];
  history[0] = total;
  drawUI();
}

// ---------- Dice-to-dice collisions ----------
void collideDice() {
  float minDist = DICE_R * 1.8f;
  for (int i = 0; i < NUM_DICE - 1; i++) {
    for (int j = i + 1; j < NUM_DICE; j++) {
      Die &a = dice[i];
      Die &b = dice[j];
      float dx = b.x - a.x;
      float dy = b.y - a.y;
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist < minDist && dist > 0.001f) {
        float nx = dx / dist;
        float ny = dy / dist;

        // push apart so they don't overlap
        float overlap = (minDist - dist) * 0.5f;
        a.x -= nx * overlap;  a.y -= ny * overlap;
        b.x += nx * overlap;  b.y += ny * overlap;

        // exchange velocity along the collision normal (equal masses)
        float rvx = b.vx - a.vx;
        float rvy = b.vy - a.vy;
        float vn = rvx * nx + rvy * ny;
        if (vn < 0) {
          float imp = -vn * 0.85f;           // restitution
          a.vx -= imp * nx;  a.vy -= imp * ny;
          b.vx += imp * nx;  b.vy += imp * ny;

          // collision kicks the spin
          a.avy += (random(-10, 11)) / 80.0f;
          b.avy += (random(-10, 11)) / 80.0f;
          a.avx += (random(-10, 11)) / 80.0f;
          b.avx += (random(-10, 11)) / 80.0f;
        }
      }
    }
  }
}

void updateDie(Die &d) {
  if (d.settled) return;
  float floorPos = FLOOR_Y - DICE_R;

  d.vy += GRAVITY;
  d.vx *= AIR_DRAG;
  d.x += d.vx;
  d.y += d.vy;

  bool onFloor = false;
  if (d.y >= floorPos) {
    d.y = floorPos;
    if (fabsf(d.vy) > 2.0f) {
      d.vy = -d.vy * BOUNCE;
      d.avx += (random(-10, 11)) / 100.0f;
      d.avz += (random(-10, 11)) / 100.0f;
    } else {
      d.vy = 0;
      onFloor = true;
    }
    d.vx *= ROLL_FRICTION;
  }

  if (d.x < LEFT_WALL + DICE_R) {
    d.x = LEFT_WALL + DICE_R;
    d.vx = -d.vx * WALL_BOUNCE;
    d.avy = -d.avy * 0.8f;
  }
  if (d.x > RIGHT_WALL - DICE_R) {
    d.x = RIGHT_WALL - DICE_R;
    d.vx = -d.vx * WALL_BOUNCE;
    d.avy = -d.avy * 0.8f;
  }

  if (onFloor) {
    d.avz = -d.vx / DICE_R * 0.9f;
    d.avx *= 0.9f;
    d.avy *= 0.9f;
  } else {
    d.avx *= SPIN_DAMP;
    d.avy *= SPIN_DAMP;
    d.avz *= SPIN_DAMP;
  }

  d.ax += d.avx;
  d.ay += d.avy;
  d.az += d.avz;

  // flickering number, slowing as it slows
  float speed = fabsf(d.vx) + fabsf(d.vy);
  unsigned long interval = 50 + (unsigned long)(constrain(8.0f - speed, 0.0f, 8.0f) * 40);
  if (millis() - d.lastNum > interval) {
    d.lastNum = millis();
    d.shown = random(1, 101);
  }

  // this die has come to rest
  if (onFloor && fabsf(d.vx) < 0.15f) {
    d.settled = true;
    d.shown = d.result;
    d.vx = 0;
    d.avx = d.avy = d.avz = 0;
  }
}

void updatePhysics() {
  for (int i = 0; i < NUM_DICE; i++) updateDie(dice[i]);
  collideDice();

  // wake up a settled die if another die hits it
  for (int i = 0; i < NUM_DICE; i++) {
    if (dice[i].settled && (fabsf(dice[i].vx) > 0.3f)) dice[i].settled = false;
  }

  bool allSettled = true;
  for (int i = 0; i < NUM_DICE; i++)
    if (!dice[i].settled) allSettled = false;
  if (allSettled) finishRoll();
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  spr.setColorDepth(16);
  if (!spr.createSprite(SCR_W, SPR_H)) {
    Serial.println("Sprite alloc failed, trying 8-bit");
    spr.setColorDepth(8);
    spr.createSprite(SCR_W, SPR_H);
  }

  randomSeed(esp_random());
  initDice();
  drawUI();
}

void loop() {
  bool touched = ts.touched();
  if (touched && !wasTouched && state != ROLLING) {
    startRoll();
  }
  wasTouched = touched;

  if (state == ROLLING) {
    updatePhysics();
  } else {
    // idle: gentle slow spin on all dice
    for (int i = 0; i < NUM_DICE; i++) {
      dice[i].ax += 0.006f + i * 0.002f;
      dice[i].ay += 0.010f - i * 0.002f;
    }
  }

  drawScene();
  delay(12);
}
