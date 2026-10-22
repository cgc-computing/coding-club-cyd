// ============================================================================
//  UIComponents -- the shared drawing vocabulary for every screen.
//
//  These are primitives, not screens: a header, a status bar, a button, a
//  panel, a signal-strength meter. Screens compose them. Keeping them here
//  means the instrument looks the same everywhere and a change to the look
//  happens in one file.
// ============================================================================

#pragma once

#include <TFT_eSPI.h>
#include "Config.h"

// A touch target with a label. Position and size are in screen pixels.
struct UIButton {
  int16_t     x = 0;
  int16_t     y = 0;
  int16_t     w = 0;
  int16_t     h = 0;
  const char* label   = "";
  const char* sub     = nullptr;   // optional smaller second line
  uint16_t    accent  = COL_CYAN;
  bool        enabled = true;
};

namespace ui {

// --- Chrome ----------------------------------------------------------------

// Title bar across the top of every screen. `title` is the screen name;
// `badge` is an optional short string shown right-aligned.
void drawHeader(TFT_eSPI& tft, const char* title, const char* badge = nullptr);

// Status bar across the bottom. Either field may be null.
void drawFooter(TFT_eSPI& tft, const char* left, const char* right = nullptr);

// Clears just the content area, leaving header and footer intact.
void clearContent(TFT_eSPI& tft);

// --- Containers ------------------------------------------------------------

// A bordered panel in the instrument style.
void drawPanel(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h,
               uint16_t accent = COL_GRID, uint16_t fill = COL_PANEL);

// --- Buttons ---------------------------------------------------------------

void drawButton(TFT_eSPI& tft, const UIButton& b, bool pressed = false);
bool buttonContains(const UIButton& b, int16_t px, int16_t py);

// --- Readouts --------------------------------------------------------------

// Four-segment signal meter. `rssi` is in dBm; stronger signals light more
// segments and shift the colour from red through yellow to green.
void drawSignalBars(TFT_eSPI& tft, int16_t x, int16_t y, int16_t h, int32_t rssi);

// The colour this RSSI should be drawn in.
uint16_t rssiColour(int32_t rssi);

// A short human label for an RSSI: "EXCELLENT", "GOOD", "FAIR", "WEAK".
const char* rssiLabel(int32_t rssi);

// --- Text ------------------------------------------------------------------

// Draws `text` centred in the box, truncating with an ellipsis if it will
// not fit. Returns the width actually drawn.
int16_t drawCenteredText(TFT_eSPI& tft, const char* text,
                         int16_t cx, int16_t cy, uint8_t font,
                         uint16_t colour, uint16_t bg);

}  // namespace ui
