// ============================================================================
//  TouchManager -- XPT2046 resistive touch: sampling, debouncing, calibration.
//
//  Raw XPT2046 readings are 12-bit ADC counts (0..4095) in the panel's own
//  orientation, which is NOT the display's orientation. Everything outside
//  this class works in screen pixels; the translation happens here.
//
//  Calibration is stored in NVS. On first boot there is nothing stored, so
//  the sketch runs runCalibration() rather than trusting the fallback
//  constants in Config.h.
// ============================================================================

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "Config.h"

// Linear map from raw ADC counts to screen pixels.
//
// xMinRaw/xMaxRaw are the raw values that correspond to screen x = 0 and
// x = SCREEN_W-1. They are stored signed and may be "reversed" (min > max):
// that is how an inverted axis is represented, and map() handles it directly.
struct TouchCalibration {
  bool    swapXY   = CAL_DEFAULT_SWAP_XY;
  int16_t xMinRaw  = CAL_DEFAULT_X_MIN;
  int16_t xMaxRaw  = CAL_DEFAULT_X_MAX;
  int16_t yMinRaw  = CAL_DEFAULT_Y_MIN;
  int16_t yMaxRaw  = CAL_DEFAULT_Y_MAX;
  bool    valid    = false;   // true only when loaded from NVS or just measured
};

class TouchManager {
public:
  // Starts VSPI on the touch pins and the XPT2046 driver, then loads any
  // stored calibration. Returns true if a stored calibration was found.
  bool begin();

  // Samples the panel and updates the press/release state machine.
  // Call once per loop() iteration. Never blocks.
  void update();

  // --- State -------------------------------------------------------------

  bool    isPressed()    const { return _stableDown; }
  bool    justPressed()  const { return _justPressed; }   // true for one update
  bool    justReleased() const { return _justReleased; }  // true for one update

  // A tap is a press and release at (roughly) the same place. True for one
  // update, on release. Use tapX()/tapY() for where it happened.
  bool    tapped()       const { return _tapped; }

  int16_t x()    const { return _x; }        // current position, screen px
  int16_t y()    const { return _y; }
  int16_t tapX() const { return _pressX; }   // where the finger went down
  int16_t tapY() const { return _pressY; }

  // Vertical movement since the last update while held down, in screen px.
  // Positive means the finger moved down the screen. Used for list scrolling.
  int16_t dragDeltaY() const { return _dragDeltaY; }

  // True once the finger has moved far enough that this is a drag, not a tap.
  bool    isDragging()  const { return _dragging; }

  // Discards the current gesture and suppresses all further events until
  // the finger has actually lifted. Call after acting on a tap that changes
  // screens, so the new screen neither re-sees that tap nor receives a
  // phantom release from a finger that was already down.
  void    consume();

  // --- Calibration -------------------------------------------------------

  bool isCalibrated() const { return _cal.valid; }

  // Modal four-corner calibration. This one routine deliberately blocks:
  // it is a setup mode, not part of the running UI. Returns true if the
  // measurement was sane and was stored.
  bool runCalibration(TFT_eSPI& tft);

  void clearCalibration();

  const TouchCalibration& calibration() const { return _cal; }

private:
  // Averages several raw samples. Returns false if the finger lifted.
  //
  // XPT2046_Touchscreen only performs a real SPI acquisition every 3 ms and
  // returns a cached reading in between, so samples taken back to back are
  // the same reading. spacingMs forces genuinely independent samples; it is
  // only worth paying for during calibration.
  bool readRaw(int16_t& rawX, int16_t& rawY,
               uint8_t samples = 1, uint8_t spacingMs = 0);

  // Applies _cal to a raw reading.
  void rawToScreen(int16_t rawX, int16_t rawY, int16_t& sx, int16_t& sy) const;

  // Blocks until a stable press is collected at a calibration target.
  bool awaitCalibrationPoint(TFT_eSPI& tft, int16_t targetX, int16_t targetY,
                             int16_t& rawX, int16_t& rawY);

  void drawCalibrationTarget(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t colour);

  bool loadCalibration();
  void saveCalibration();

  SPIClass            _spi{VSPI};
  XPT2046_Touchscreen _ts{PIN_TOUCH_CS,
                          TOUCH_USE_IRQ ? PIN_TOUCH_IRQ : 255};
  TouchCalibration    _cal;

  // Debounce / gesture state
  bool     _rawDown      = false;
  bool     _stableDown   = false;
  bool     _justPressed  = false;
  bool     _justReleased = false;
  bool     _tapped       = false;
  bool     _dragging     = false;

  // Set by consume(). While true, every reading is discarded until the
  // panel reads clear, so one gesture can never span two screens.
  bool     _ignoreUntilRelease = false;

  int16_t  _x = 0,      _y = 0;
  int16_t  _pressX = 0, _pressY = 0;
  int16_t  _lastY = 0;
  int16_t  _dragDeltaY = 0;

  uint32_t _lastEdgeMs    = 0;
  uint32_t _lastReleaseMs = 0;
};
