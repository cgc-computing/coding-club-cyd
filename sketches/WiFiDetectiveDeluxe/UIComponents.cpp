#include "UIComponents.h"

namespace ui {

// ---------------------------------------------------------------------------
//  Chrome
// ---------------------------------------------------------------------------

void drawHeader(TFT_eSPI& tft, const char* title, const char* badge) {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, COL_PANEL);
  tft.drawFastHLine(0, HEADER_H - 1, SCREEN_W, COL_CYAN);

  // A short accent block at the left edge reads as an instrument bezel.
  tft.fillRect(0, 0, 4, HEADER_H - 1, COL_CYAN);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_CYAN, COL_PANEL);
  tft.drawString(title, 10, HEADER_H / 2 - 1, 2);

  if (badge != nullptr) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COL_TEXT_DIM, COL_PANEL);
    tft.drawString(badge, SCREEN_W - 8, HEADER_H / 2 - 1, 2);
  }
}

void drawFooter(TFT_eSPI& tft, const char* left, const char* right) {
  tft.fillRect(0, FOOTER_Y, SCREEN_W, FOOTER_H, COL_PANEL);
  tft.drawFastHLine(0, FOOTER_Y, SCREEN_W, COL_GRID);

  if (left != nullptr) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TEXT_DIM, COL_PANEL);
    tft.drawString(left, 8, FOOTER_Y + FOOTER_H / 2 - 1, 2);
  }
  if (right != nullptr) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COL_TEXT_DIM, COL_PANEL);
    tft.drawString(right, SCREEN_W - 8, FOOTER_Y + FOOTER_H / 2 - 1, 2);
  }
}

void clearContent(TFT_eSPI& tft) {
  tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
}

// ---------------------------------------------------------------------------
//  Containers
// ---------------------------------------------------------------------------

void drawPanel(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h,
               uint16_t accent, uint16_t fill) {
  tft.fillRoundRect(x, y, w, h, 4, fill);
  tft.drawRoundRect(x, y, w, h, 4, accent);
}

// ---------------------------------------------------------------------------
//  Buttons
// ---------------------------------------------------------------------------

void drawButton(TFT_eSPI& tft, const UIButton& b, bool pressed) {
  const uint16_t accent = b.enabled ? b.accent : COL_TEXT_DIM;
  const uint16_t fill   = pressed ? COL_PANEL_HI : COL_PANEL;
  const uint16_t text   = b.enabled ? (pressed ? COL_WHITE : accent)
                                    : COL_TEXT_DIM;

  tft.fillRoundRect(b.x, b.y, b.w, b.h, 5, fill);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 5, accent);

  // A pressed button gets a second inner outline rather than a colour flip,
  // so the change is obvious without repainting the whole area twice.
  if (pressed) {
    tft.drawRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 4, accent);
  }

  // Accent stripe down the left edge of the button.
  tft.fillRect(b.x + 2, b.y + 6, 3, b.h - 12, accent);

  const int16_t textX = b.x + b.w / 2 + 2;
  if (b.sub != nullptr) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(text, fill);
    tft.drawString(b.label, textX, b.y + b.h / 2 - 9, 2);
    tft.setTextColor(COL_TEXT_DIM, fill);
    tft.drawString(b.sub, textX, b.y + b.h / 2 + 10, 1);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(text, fill);
    tft.drawString(b.label, textX, b.y + b.h / 2 - 1, 2);
  }
}

bool buttonContains(const UIButton& b, int16_t px, int16_t py) {
  if (!b.enabled) return false;
  return px >= b.x && px < b.x + b.w &&
         py >= b.y && py < b.y + b.h;
}

// ---------------------------------------------------------------------------
//  Readouts
// ---------------------------------------------------------------------------

uint16_t rssiColour(int32_t rssi) {
  if (rssi >= -55) return COL_GOOD;
  if (rssi >= -67) return COL_FAIR;
  if (rssi >= -78) return COL_WARN;
  return COL_BAD;
}

const char* rssiLabel(int32_t rssi) {
  if (rssi >= -55) return "EXCELLENT";
  if (rssi >= -67) return "GOOD";
  if (rssi >= -78) return "FAIR";
  return "WEAK";
}

void drawSignalBars(TFT_eSPI& tft, int16_t x, int16_t y, int16_t h,
                    int32_t rssi) {
  constexpr int16_t BARS      = 4;
  constexpr int16_t BAR_W     = 4;
  constexpr int16_t BAR_GAP   = 2;

  // -90 dBm or worse lights nothing; -50 or better lights everything.
  int16_t lit = 0;
  if      (rssi >= -55) lit = 4;
  else if (rssi >= -67) lit = 3;
  else if (rssi >= -78) lit = 2;
  else if (rssi >= -90) lit = 1;

  const uint16_t on = rssiColour(rssi);

  for (int16_t i = 0; i < BARS; i++) {
    // Each bar is taller than the last, like a classic signal meter.
    const int16_t barH = (h * (i + 1)) / BARS;
    const int16_t barX = x + i * (BAR_W + BAR_GAP);
    const int16_t barY = y + (h - barH);

    if (i < lit) {
      tft.fillRect(barX, barY, BAR_W, barH, on);
    } else {
      tft.drawRect(barX, barY, BAR_W, barH, COL_GRID);
    }
  }
}

// ---------------------------------------------------------------------------
//  Text
// ---------------------------------------------------------------------------

int16_t drawCenteredText(TFT_eSPI& tft, const char* text,
                         int16_t cx, int16_t cy, uint8_t font,
                         uint16_t colour, uint16_t bg) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(colour, bg);
  return tft.drawString(text, cx, cy, font);
}

}  // namespace ui
