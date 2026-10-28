// ============================================================================
//  DisplayManager -- owns the TFT_eSPI instance and the backlight.
//
//  Every other module draws through the TFT_eSPI reference this hands out.
//  Keeping ownership in one place means initialisation order and the
//  CYD-specific quirks (invert, rotation, backlight PWM) live in exactly
//  one file.
// ============================================================================

#pragma once

#include <TFT_eSPI.h>
#include "Config.h"

class DisplayManager {
public:
  // Brings up the panel: backlight PWM, TFT init, rotation, colour invert.
  void begin();

  // The raw drawing surface. Modules use this directly for speed; there is
  // no value in wrapping several hundred TFT_eSPI calls.
  TFT_eSPI& tft() { return _tft; }

  // Backlight duty, 0..255. Values below BACKLIGHT_MINIMUM are clamped so
  // the screen can never be turned fully black by a bad setting.
  void     setBrightness(uint8_t duty);
  uint8_t  brightness() const { return _brightness; }

  int16_t  width()  const { return SCREEN_W; }
  int16_t  height() const { return SCREEN_H; }

private:
  TFT_eSPI _tft;
  uint8_t  _brightness = BACKLIGHT_DEFAULT;
};
