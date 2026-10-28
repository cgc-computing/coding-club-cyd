#include "DisplayManager.h"

void DisplayManager::begin() {
  // Backlight first, on PWM, so the panel is lit while the TFT initialises
  // and we never flash a bright white screen at the user.
  //
  // ledcAttach() is the ESP32 Arduino core 3.x API. On core 2.x this call
  // does not exist -- see README.md for the required core version.
  ledcAttach(PIN_TFT_BACKLIGHT, BACKLIGHT_PWM_FREQ_HZ, BACKLIGHT_PWM_BITS);
  setBrightness(_brightness);

  _tft.init();
  _tft.setRotation(TFT_ROTATION);

  // Our CYD revision drives the panel with inverted colour. Without this the
  // whole UI comes out as a photographic negative.
  if (TFT_INVERT) {
    _tft.invertDisplay(true);
  }

  _tft.fillScreen(COL_BG);
  _tft.setTextWrap(false);
}

void DisplayManager::setBrightness(uint8_t duty) {
  _brightness = (duty < BACKLIGHT_MINIMUM) ? BACKLIGHT_MINIMUM : duty;
  ledcWrite(PIN_TFT_BACKLIGHT, _brightness);
}
