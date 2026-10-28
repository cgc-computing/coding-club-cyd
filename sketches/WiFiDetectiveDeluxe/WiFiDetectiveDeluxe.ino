// ============================================================================
//  Wi-Fi Detective Deluxe -- a handheld Wi-Fi analyser for the ESP32 CYD
//  (ESP32-2432S028R, 320x240 landscape, ILI9341 + XPT2046 resistive touch)
//
//  PHASE 1: hardware bring-up, touch calibration, main menu, screen routing.
//
//  The six feature screens are reachable and drawn, but they announce which
//  phase will implement them rather than showing invented data. Nothing in
//  this sketch pretends to have information it does not have.
//
//  Scope: passive discovery and analysis only. This sketch never transmits
//  on behalf of, or interferes with, any network it observes.
//
//  Coding Club -- CGC Computing Studies
// ============================================================================

#include <WiFi.h>

#include "Config.h"
#include "DisplayManager.h"
#include "TouchManager.h"
#include "UIComponents.h"

// ---------------------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------------------

DisplayManager display;
TouchManager   touch;

// Every screen the application can show. Navigation is a plain state machine:
// one current screen, plus a flag saying whether it still needs its first
// full paint. Nothing draws except through this.
enum ScreenId : uint8_t {
  SCR_SPLASH,
  SCR_MENU,
  SCR_NETWORKS,
  SCR_CHANNELS,
  SCR_RADAR,
  SCR_DETAILS,
  SCR_MONITOR,
  SCR_SETTINGS,
  SCR_COUNT
};

ScreenId currentScreen = SCR_SPLASH;
bool     screenIsNew   = true;      // true until the screen has painted once

uint32_t screenEnteredMs = 0;

// Which on-screen button the finger is currently inside, or -1. Used purely
// for the pressed-state highlight.
int8_t pressedButton = -1;

// ---------------------------------------------------------------------------
//  Main menu definition
// ---------------------------------------------------------------------------

constexpr uint8_t MENU_COUNT = 6;
constexpr uint8_t MENU_COLS  = 2;
constexpr uint8_t MENU_ROWS  = 3;
constexpr int16_t MENU_GAP   = 8;

UIButton menuButtons[MENU_COUNT];

struct MenuEntry {
  const char* label;
  const char* sub;
  uint16_t    accent;
  ScreenId    target;
};

const MenuEntry MENU_ENTRIES[MENU_COUNT] = {
  {"NETWORKS",     "discovered APs",  COL_CYAN, SCR_NETWORKS},
  {"CHANNELS",     "2.4 GHz usage",   COL_GOOD, SCR_CHANNELS},
  {"RADAR",        "visualisation",   COL_FAIR, SCR_RADAR},
  {"DETAILS",      "selected AP",     COL_CYAN, SCR_DETAILS},
  {"LIVE MONITOR", "track one BSSID", COL_WARN, SCR_MONITOR},
  {"SETTINGS",     "device options",  COL_TEXT, SCR_SETTINGS},
};

// What each not-yet-built screen should say for itself.
struct StubInfo {
  const char* title;
  const char* phase;
  const char* detail;
};

const StubInfo STUB_INFO[SCR_COUNT] = {
  /* SPLASH   */ {"", "", ""},
  /* MENU     */ {"", "", ""},
  /* NETWORKS */ {"NETWORKS", "PHASE 2", "Scan, list and sort nearby APs"},
  /* CHANNELS */ {"CHANNELS", "PHASE 3", "2.4 GHz occupancy and overlap"},
  /* RADAR    */ {"RADAR",    "PHASE 4", "Animated signal visualisation"},
  /* DETAILS  */ {"DETAILS",  "PHASE 2", "Full record for one access point"},
  /* MONITOR  */ {"LIVE MONITOR", "PHASE 5", "RSSI history for one BSSID"},
  /* SETTINGS */ {"SETTINGS", "PHASE 6", "Brightness, theme, scan interval"},
};

// The BACK button, shared by every sub-screen.
UIButton backButton;

// The one control on the Settings stub that genuinely works in Phase 1.
UIButton recalibrateButton;

// ---------------------------------------------------------------------------
//  Status line helpers
// ---------------------------------------------------------------------------

// Free heap in KiB. Shown as a diagnostic so a memory leak introduced in a
// later phase is visible on the device itself.
void formatHeap(char* out, size_t len) {
  snprintf(out, len, "HEAP %uK",
           static_cast<unsigned>(ESP.getFreeHeap() / 1024));
}

// Phase 1 brings the radio up but does not scan, so the counters honestly
// report that no scan has happened yet.
void formatScanStatus(char* out, size_t len) {
  snprintf(out, len, "WIFI READY  NETS --  SCAN NEVER");
}

// ---------------------------------------------------------------------------
//  Navigation
// ---------------------------------------------------------------------------

void gotoScreen(ScreenId next) {
  currentScreen   = next;
  screenIsNew     = true;
  screenEnteredMs = millis();
  pressedButton   = -1;
  touch.consume();          // the tap that got us here must not act again
}

// ---------------------------------------------------------------------------
//  Layout
// ---------------------------------------------------------------------------

void layoutMenu() {
  const int16_t bw = (SCREEN_W - MENU_GAP * (MENU_COLS + 1)) / MENU_COLS;
  const int16_t bh = (CONTENT_H - MENU_GAP * (MENU_ROWS + 1)) / MENU_ROWS;

  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    const uint8_t col = i % MENU_COLS;
    const uint8_t row = i / MENU_COLS;

    menuButtons[i].x      = MENU_GAP + col * (bw + MENU_GAP);
    menuButtons[i].y      = CONTENT_Y + MENU_GAP + row * (bh + MENU_GAP);
    menuButtons[i].w      = bw;
    menuButtons[i].h      = bh;
    menuButtons[i].label  = MENU_ENTRIES[i].label;
    menuButtons[i].sub    = MENU_ENTRIES[i].sub;
    menuButtons[i].accent = MENU_ENTRIES[i].accent;
    menuButtons[i].enabled = true;
  }
}

void layoutSubScreenButtons() {
  backButton.x      = MENU_GAP;
  backButton.y      = CONTENT_Y + CONTENT_H - 44;
  backButton.w      = 100;
  backButton.h      = 36;
  backButton.label  = "< BACK";
  backButton.sub    = nullptr;
  backButton.accent = COL_CYAN;

  recalibrateButton.x      = SCREEN_W - MENU_GAP - 140;
  recalibrateButton.y      = backButton.y;
  recalibrateButton.w      = 140;
  recalibrateButton.h      = 36;
  recalibrateButton.label  = "RECALIBRATE";
  recalibrateButton.sub    = nullptr;
  recalibrateButton.accent = COL_WARN;
}

// ---------------------------------------------------------------------------
//  Screen: splash
// ---------------------------------------------------------------------------

constexpr uint32_t SPLASH_DURATION_MS = 2200;

void drawSplash() {
  TFT_eSPI& tft = display.tft();
  tft.fillScreen(COL_BG);

  // Scan-line texture: cheap, and it sells the "instrument" look.
  for (int16_t y = 0; y < SCREEN_H; y += 4) {
    tft.drawFastHLine(0, y, SCREEN_W, COL_GRID);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_CYAN, COL_BG);
  tft.drawString("WI-FI", SCREEN_W / 2, 62, 6);

  tft.setTextColor(COL_WHITE, COL_BG);
  tft.drawString("DETECTIVE", SCREEN_W / 2, 108, 4);

  tft.setTextColor(COL_WARN, COL_BG);
  tft.drawString("D E L U X E", SCREEN_W / 2, 138, 2);

  tft.drawFastHLine(60, 158, SCREEN_W - 120, COL_CYAN_DIM);

  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.drawString(APP_VERSION, SCREEN_W / 2, 176, 1);
  tft.drawString("PASSIVE DISCOVERY -- CGC CODING CLUB",
                 SCREEN_W / 2, 192, 1);

  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.drawString("tap to continue", SCREEN_W / 2, 224, 1);
}

void updateSplashProgress() {
  TFT_eSPI& tft = display.tft();

  const uint32_t elapsed = millis() - screenEnteredMs;
  const int16_t  barX    = 60;
  const int16_t  barW    = SCREEN_W - 120;
  const int16_t  barY    = 206;

  int32_t filled = (static_cast<int32_t>(elapsed) * barW) / SPLASH_DURATION_MS;
  if (filled > barW) filled = barW;

  tft.fillRect(barX, barY, filled, 3, COL_CYAN);
}

void screenSplash() {
  if (screenIsNew) {
    drawSplash();
    screenIsNew = false;
  }

  updateSplashProgress();

  const bool elapsed = (millis() - screenEnteredMs) >= SPLASH_DURATION_MS;
  if (elapsed || touch.tapped()) {
    gotoScreen(SCR_MENU);
  }
}

// ---------------------------------------------------------------------------
//  Screen: main menu
// ---------------------------------------------------------------------------

uint32_t lastFooterUpdateMs = 0;
constexpr uint32_t FOOTER_REFRESH_MS = 1000;

void drawMenuFooter() {
  char status[48];
  char heap[16];
  formatScanStatus(status, sizeof(status));
  formatHeap(heap, sizeof(heap));
  ui::drawFooter(display.tft(), status, heap);
  lastFooterUpdateMs = millis();
}

void screenMenu() {
  TFT_eSPI& tft = display.tft();

  if (screenIsNew) {
    tft.fillScreen(COL_BG);
    ui::drawHeader(tft, APP_NAME, "P1");
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
      ui::drawButton(tft, menuButtons[i], false);
    }
    drawMenuFooter();
    screenIsNew = false;
  }

  // Only the footer changes while the menu sits idle, so only the footer is
  // repainted. Nothing else on this screen is touched -- no flicker.
  if (millis() - lastFooterUpdateMs >= FOOTER_REFRESH_MS) {
    drawMenuFooter();
  }

  // --- Pressed-state feedback --------------------------------------------
  if (touch.isPressed()) {
    int8_t over = -1;
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
      if (ui::buttonContains(menuButtons[i], touch.x(), touch.y())) {
        over = static_cast<int8_t>(i);
        break;
      }
    }
    if (over != pressedButton) {
      if (pressedButton >= 0) {
        ui::drawButton(tft, menuButtons[pressedButton], false);
      }
      if (over >= 0) {
        ui::drawButton(tft, menuButtons[over], true);
      }
      pressedButton = over;
    }
  } else if (pressedButton >= 0 && !touch.tapped()) {
    // Finger lifted outside any button, or the gesture became a drag.
    ui::drawButton(tft, menuButtons[pressedButton], false);
    pressedButton = -1;
  }

  // --- Activation ---------------------------------------------------------
  // Acting on release rather than on press means a finger that lands on the
  // wrong button can slide off without triggering it.
  if (touch.tapped()) {
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
      if (ui::buttonContains(menuButtons[i], touch.tapX(), touch.tapY())) {
        gotoScreen(MENU_ENTRIES[i].target);
        return;
      }
    }
    if (pressedButton >= 0) {
      ui::drawButton(tft, menuButtons[pressedButton], false);
      pressedButton = -1;
    }
  }
}

// ---------------------------------------------------------------------------
//  Screens: the six feature screens (Phase 1 placeholders)
//
//  These draw a real screen with working navigation, and state plainly that
//  the feature itself is not built yet. They do not fabricate networks,
//  signal levels or channel data.
// ---------------------------------------------------------------------------

void drawStubScreen(ScreenId id) {
  TFT_eSPI& tft = display.tft();
  const StubInfo& info = STUB_INFO[id];

  tft.fillScreen(COL_BG);
  ui::drawHeader(tft, info.title, info.phase);

  const int16_t panelX = MENU_GAP;
  const int16_t panelY = CONTENT_Y + MENU_GAP;
  const int16_t panelW = SCREEN_W - 2 * MENU_GAP;
  const int16_t panelH = CONTENT_H - 3 * MENU_GAP - 36;

  ui::drawPanel(tft, panelX, panelY, panelW, panelH, COL_GRID);

  const int16_t cx = panelX + panelW / 2;

  ui::drawCenteredText(tft, "NOT IMPLEMENTED YET",
                       cx, panelY + 30, 4, COL_WARN, COL_PANEL);
  ui::drawCenteredText(tft, info.detail,
                       cx, panelY + 58, 2, COL_TEXT, COL_PANEL);
  ui::drawCenteredText(tft, "arrives in", cx, panelY + 82, 1, COL_TEXT_DIM, COL_PANEL);
  ui::drawCenteredText(tft, info.phase, cx, panelY + 97, 2, COL_CYAN, COL_PANEL);

  ui::drawButton(tft, backButton, false);

  if (id == SCR_SETTINGS) {
    // Touch calibration is Phase 1 work, so this button really does run.
    ui::drawButton(tft, recalibrateButton, false);
    ui::drawFooter(tft, "Calibration is available now", nullptr);
  } else {
    ui::drawFooter(tft, "Touch navigation is working", nullptr);
  }
}

void screenStub(ScreenId id) {
  TFT_eSPI& tft = display.tft();

  if (screenIsNew) {
    drawStubScreen(id);
    screenIsNew = false;
  }

  const bool settings = (id == SCR_SETTINGS);

  // Pressed-state feedback for the one or two buttons on this screen.
  if (touch.isPressed()) {
    int8_t over = -1;
    if (ui::buttonContains(backButton, touch.x(), touch.y())) {
      over = 0;
    } else if (settings &&
               ui::buttonContains(recalibrateButton, touch.x(), touch.y())) {
      over = 1;
    }
    if (over != pressedButton) {
      if (pressedButton == 0) ui::drawButton(tft, backButton, false);
      if (pressedButton == 1) ui::drawButton(tft, recalibrateButton, false);
      if (over == 0)          ui::drawButton(tft, backButton, true);
      if (over == 1)          ui::drawButton(tft, recalibrateButton, true);
      pressedButton = over;
    }
  } else if (pressedButton >= 0 && !touch.tapped()) {
    if (pressedButton == 0) ui::drawButton(tft, backButton, false);
    if (pressedButton == 1) ui::drawButton(tft, recalibrateButton, false);
    pressedButton = -1;
  }

  if (touch.tapped()) {
    if (ui::buttonContains(backButton, touch.tapX(), touch.tapY())) {
      gotoScreen(SCR_MENU);
      return;
    }
    if (settings &&
        ui::buttonContains(recalibrateButton, touch.tapX(), touch.tapY())) {
      // runCalibration() is modal and takes over the screen. Retry until it
      // produces a sane result, so the device can never be left unusable.
      while (!touch.runCalibration(display.tft())) {
        delay(300);
      }
      gotoScreen(SCR_SETTINGS);
      return;
    }
  }
}

// ---------------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.printf("%s %s\n", APP_NAME, APP_VERSION);

  display.begin();
  layoutMenu();
  layoutSubScreenButtons();

  const bool hadCalibration = touch.begin();
  Serial.printf("Touch calibration in NVS: %s\n",
                hadCalibration ? "found" : "none");

  // Bring the radio up now so the Wi-Fi status shown on the menu is real.
  // Phase 1 does not scan; Phase 2 will.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);

  if (!hadCalibration) {
    // Nothing usable is stored, and the fallback constants in Config.h are
    // generic rather than measured. Calibrate before showing any UI.
    TFT_eSPI& tft = display.tft();
    tft.fillScreen(COL_BG);
    ui::drawCenteredText(tft, "FIRST RUN", SCREEN_W / 2, SCREEN_H / 2 - 16,
                         4, COL_CYAN, COL_BG);
    ui::drawCenteredText(tft, "Touch calibration is required",
                         SCREEN_W / 2, SCREEN_H / 2 + 14, 2, COL_TEXT, COL_BG);
    delay(1400);

    while (!touch.runCalibration(display.tft())) {
      delay(300);
    }
  }

  const TouchCalibration& cal = touch.calibration();
  Serial.printf("Calibration: swapXY=%d x[%d..%d] y[%d..%d]\n",
                cal.swapXY, cal.xMinRaw, cal.xMaxRaw, cal.yMinRaw, cal.yMaxRaw);
  Serial.printf("Free heap: %u bytes\n",
                static_cast<unsigned>(ESP.getFreeHeap()));

  gotoScreen(SCR_SPLASH);
}

void loop() {
  touch.update();

  switch (currentScreen) {
    case SCR_SPLASH:
      screenSplash();
      break;

    case SCR_MENU:
      screenMenu();
      break;

    case SCR_NETWORKS:
    case SCR_CHANNELS:
    case SCR_RADAR:
    case SCR_DETAILS:
    case SCR_MONITOR:
    case SCR_SETTINGS:
      screenStub(currentScreen);
      break;

    default:
      gotoScreen(SCR_MENU);
      break;
  }

  // A short yield keeps the touch sampling rate sane (~60 Hz) without
  // spinning the CPU on SPI reads.
  delay(16);
}
