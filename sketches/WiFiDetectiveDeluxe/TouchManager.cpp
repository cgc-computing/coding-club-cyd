#include "TouchManager.h"
#include <Preferences.h>

namespace {

// Calibration targets sit this far in from each edge. Far enough from the
// bezel that the panel still reads linearly there, close enough that
// extrapolating out to the corners stays accurate.
constexpr int16_t CAL_INSET = 24;

// A calibration is rejected if the raw span across the screen is smaller
// than this. A real panel spans roughly 3000 counts; anything this small
// means the user tapped the same spot four times, or the wiring is wrong.
constexpr int16_t CAL_MIN_SPAN = 400;

constexpr uint8_t CAL_SAMPLES    = 16;   // raw reads averaged per target
constexpr uint16_t CAL_SETTLE_MS = 120;  // let the finger settle before sampling

// The driver acquires at most once every 3 ms, so samples must be spaced at
// least that far apart to be independent of each other.
constexpr uint8_t CAL_SAMPLE_GAP_MS = 5;

int16_t clamp16(int32_t v, int16_t lo, int16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return static_cast<int16_t>(v);
}

}  // namespace

// ---------------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------------

bool TouchManager::begin() {
  // The touch panel has its own SPI bus -- see the note in Config.h.
  _spi.begin(PIN_TOUCH_CLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  _ts.begin(_spi);
  _ts.setRotation(1);   // rotation 1 is pass-through: we do our own mapping

  return loadCalibration();
}

// ---------------------------------------------------------------------------
//  Sampling
// ---------------------------------------------------------------------------

bool TouchManager::readRaw(int16_t& rawX, int16_t& rawY,
                           uint8_t samples, uint8_t spacingMs) {
  if (samples == 0) samples = 1;

  int32_t sumX = 0;
  int32_t sumY = 0;
  uint8_t got  = 0;

  for (uint8_t i = 0; i < samples; i++) {
    // Without this wait the driver hands back the reading it already has,
    // so every extra "sample" would be a duplicate and the average would
    // be no better than a single read.
    if (i > 0 && spacingMs > 0) {
      delay(spacingMs);
    }
    if (!_ts.touched()) break;
    TS_Point p = _ts.getPoint();
    sumX += p.x;
    sumY += p.y;
    got++;
  }

  if (got == 0) return false;

  rawX = static_cast<int16_t>(sumX / got);
  rawY = static_cast<int16_t>(sumY / got);
  return true;
}

void TouchManager::rawToScreen(int16_t rawX, int16_t rawY,
                               int16_t& sx, int16_t& sy) const {
  // swapXY decides which raw axis feeds which screen axis. Inversion is
  // already encoded in the min/max ordering, so map() needs no extra work.
  const int16_t srcX = _cal.swapXY ? rawY : rawX;
  const int16_t srcY = _cal.swapXY ? rawX : rawY;

  sx = clamp16(map(srcX, _cal.xMinRaw, _cal.xMaxRaw, 0, SCREEN_W - 1),
               0, SCREEN_W - 1);
  sy = clamp16(map(srcY, _cal.yMinRaw, _cal.yMaxRaw, 0, SCREEN_H - 1),
               0, SCREEN_H - 1);
}

// ---------------------------------------------------------------------------
//  Press / release state machine
// ---------------------------------------------------------------------------

void TouchManager::update() {
  // These are single-update events; clear them before deciding anew.
  _justPressed  = false;
  _justReleased = false;
  _tapped       = false;
  _dragDeltaY   = 0;

  const uint32_t now = millis();

  int16_t rawX = 0, rawY = 0;
  const bool downNow = readRaw(rawX, rawY);

  // A resistive panel chatters on contact and release. Ignore any change of
  // state until the signal has held steady for TOUCH_DEBOUNCE_MS.
  if (downNow != _rawDown) {
    _rawDown     = downNow;
    _lastEdgeMs  = now;
  }

  const bool settled = (now - _lastEdgeMs) >= TOUCH_DEBOUNCE_MS;

  // A gesture that was consumed on a previous screen must not produce a
  // release or tap on this one. Wait for a genuinely clear panel first.
  if (_ignoreUntilRelease) {
    if (!downNow) {
      _ignoreUntilRelease = false;
      _lastReleaseMs      = now;
    }
    _stableDown = false;
    _dragging   = false;
    return;
  }

  if (downNow) {
    int16_t sx, sy;
    rawToScreen(rawX, rawY, sx, sy);

    if (!_stableDown && settled) {
      // Guard against a single press being reported twice when the panel
      // briefly loses contact under a moving finger.
      if (now - _lastReleaseMs < TOUCH_REPRESS_MS) {
        return;
      }
      _stableDown  = true;
      _justPressed = true;
      _dragging    = false;
      _pressX = sx;
      _pressY = sy;
      _lastY  = sy;
    }

    if (_stableDown) {
      _dragDeltaY = sy - _lastY;
      _lastY      = sy;
      _x = sx;
      _y = sy;

      if (!_dragging) {
        const int16_t movedX = abs(sx - _pressX);
        const int16_t movedY = abs(sy - _pressY);
        if (movedX > TOUCH_SCROLL_MIN_PX || movedY > TOUCH_SCROLL_MIN_PX) {
          _dragging = true;
        }
      }
    }
  } else if (_stableDown && settled) {
    _stableDown    = false;
    _justReleased  = true;
    _lastReleaseMs = now;

    // A tap is a release that never wandered far from where it started.
    const int16_t movedX = abs(_x - _pressX);
    const int16_t movedY = abs(_y - _pressY);
    _tapped = (movedX <= TOUCH_TAP_SLOP_PX && movedY <= TOUCH_TAP_SLOP_PX);

    _dragging = false;
  }
}

void TouchManager::consume() {
  _justPressed  = false;
  _justReleased = false;
  _tapped       = false;
  _dragging     = false;
  _dragDeltaY   = 0;
  _stableDown   = false;

  // If the finger is still down, suppress events until it lifts.
  _ignoreUntilRelease = true;
}

// ---------------------------------------------------------------------------
//  Calibration storage
// ---------------------------------------------------------------------------

bool TouchManager::loadCalibration() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {   // read-only
    return false;
  }

  const bool present = prefs.getBool("calOK", false);
  if (present) {
    _cal.swapXY  = prefs.getBool ("calSwap", CAL_DEFAULT_SWAP_XY);
    _cal.xMinRaw = prefs.getShort("calXMin", CAL_DEFAULT_X_MIN);
    _cal.xMaxRaw = prefs.getShort("calXMax", CAL_DEFAULT_X_MAX);
    _cal.yMinRaw = prefs.getShort("calYMin", CAL_DEFAULT_Y_MIN);
    _cal.yMaxRaw = prefs.getShort("calYMax", CAL_DEFAULT_Y_MAX);

    // Refuse a stored calibration that could not have come from a real panel.
    const bool sane = abs(_cal.xMaxRaw - _cal.xMinRaw) >= CAL_MIN_SPAN &&
                      abs(_cal.yMaxRaw - _cal.yMinRaw) >= CAL_MIN_SPAN;
    _cal.valid = sane;
  }
  prefs.end();

  return _cal.valid;
}

void TouchManager::saveCalibration() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write
    return;
  }
  prefs.putBool ("calSwap", _cal.swapXY);
  prefs.putShort("calXMin", _cal.xMinRaw);
  prefs.putShort("calXMax", _cal.xMaxRaw);
  prefs.putShort("calYMin", _cal.yMinRaw);
  prefs.putShort("calYMax", _cal.yMaxRaw);
  prefs.putBool ("calOK",   true);
  prefs.end();
}

void TouchManager::clearCalibration() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.putBool("calOK", false);
    prefs.end();
  }
  _cal = TouchCalibration();   // back to the Config.h fallbacks
  _cal.valid = false;
}

// ---------------------------------------------------------------------------
//  Calibration routine
// ---------------------------------------------------------------------------

void TouchManager::drawCalibrationTarget(TFT_eSPI& tft, int16_t cx, int16_t cy,
                                         uint16_t colour) {
  tft.drawLine(cx - 10, cy, cx + 10, cy, colour);
  tft.drawLine(cx, cy - 10, cx, cy + 10, colour);
  tft.drawCircle(cx, cy, 7, colour);
  tft.drawCircle(cx, cy, 2, colour);
}

bool TouchManager::awaitCalibrationPoint(TFT_eSPI& tft,
                                         int16_t targetX, int16_t targetY,
                                         int16_t& rawX, int16_t& rawY) {
  drawCalibrationTarget(tft, targetX, targetY, COL_CYAN);

  // Wait for the panel to be clear, so the previous target's release is not
  // mistaken for this target's press.
  while (_ts.touched()) {
    delay(10);
  }
  delay(TOUCH_DEBOUNCE_MS);

  // Wait for a press.
  while (!_ts.touched()) {
    delay(10);
  }

  delay(CAL_SETTLE_MS);          // let the contact stabilise under the finger
  const bool ok = readRaw(rawX, rawY, CAL_SAMPLES, CAL_SAMPLE_GAP_MS);

  drawCalibrationTarget(tft, targetX, targetY, ok ? COL_GOOD : COL_BAD);

  while (_ts.touched()) {        // wait for lift before returning
    delay(10);
  }
  delay(TOUCH_DEBOUNCE_MS);

  return ok;
}

bool TouchManager::runCalibration(TFT_eSPI& tft) {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_CYAN, COL_BG);
  tft.drawString("TOUCH CALIBRATION", SCREEN_W / 2, 70, 4);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Tap the centre of each target", SCREEN_W / 2, 105, 2);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.drawString("A fingertip is fine -- be as precise as you can",
                 SCREEN_W / 2, 125, 2);

  // Order: top-left, top-right, bottom-right, bottom-left.
  const int16_t targets[4][2] = {
    {CAL_INSET,              CAL_INSET},
    {SCREEN_W - 1 - CAL_INSET, CAL_INSET},
    {SCREEN_W - 1 - CAL_INSET, SCREEN_H - 1 - CAL_INSET},
    {CAL_INSET,              SCREEN_H - 1 - CAL_INSET},
  };

  int16_t rx[4] = {0, 0, 0, 0};
  int16_t ry[4] = {0, 0, 0, 0};

  for (uint8_t i = 0; i < 4; i++) {
    if (!awaitCalibrationPoint(tft, targets[i][0], targets[i][1], rx[i], ry[i])) {
      tft.fillScreen(COL_BG);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COL_BAD, COL_BG);
      tft.drawString("NO READING -- RETRYING", SCREEN_W / 2, SCREEN_H / 2, 2);
      delay(1200);
      return false;
    }
  }

  // Which raw axis tracks the screen's horizontal? Compare how much each
  // raw axis changed between the two top targets, which differ only in
  // screen x. The axis that moved more is the horizontal one.
  const int16_t deltaRawX = abs(rx[1] - rx[0]);
  const int16_t deltaRawY = abs(ry[1] - ry[0]);
  _cal.swapXY = (deltaRawY > deltaRawX);

  // Average the two readings on each edge to cancel out a slightly
  // off-centre tap.
  int32_t leftEdge, rightEdge, topEdge, bottomEdge;
  if (!_cal.swapXY) {
    leftEdge   = (rx[0] + rx[3]) / 2;
    rightEdge  = (rx[1] + rx[2]) / 2;
    topEdge    = (ry[0] + ry[1]) / 2;
    bottomEdge = (ry[2] + ry[3]) / 2;
  } else {
    leftEdge   = (ry[0] + ry[3]) / 2;
    rightEdge  = (ry[1] + ry[2]) / 2;
    topEdge    = (rx[0] + rx[1]) / 2;
    bottomEdge = (rx[2] + rx[3]) / 2;
  }

  const int32_t spanX = rightEdge - leftEdge;
  const int32_t spanY = bottomEdge - topEdge;

  if (abs(spanX) < CAL_MIN_SPAN || abs(spanY) < CAL_MIN_SPAN) {
    tft.fillScreen(COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_BAD, COL_BG);
    tft.drawString("CALIBRATION FAILED", SCREEN_W / 2, SCREEN_H / 2 - 12, 4);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("Targets read too close together",
                   SCREEN_W / 2, SCREEN_H / 2 + 20, 2);
    delay(2000);
    return false;
  }

  // The targets sit CAL_INSET from the edges, so extrapolate outwards to get
  // the raw values at screen 0 and screen W-1 / H-1.
  const int32_t pxSpanX = (SCREEN_W - 1) - 2 * CAL_INSET;
  const int32_t pxSpanY = (SCREEN_H - 1) - 2 * CAL_INSET;

  _cal.xMinRaw = static_cast<int16_t>(leftEdge - (spanX * CAL_INSET) / pxSpanX);
  _cal.xMaxRaw = static_cast<int16_t>(rightEdge + (spanX * CAL_INSET) / pxSpanX);
  _cal.yMinRaw = static_cast<int16_t>(topEdge - (spanY * CAL_INSET) / pxSpanY);
  _cal.yMaxRaw = static_cast<int16_t>(bottomEdge + (spanY * CAL_INSET) / pxSpanY);
  _cal.valid   = true;

  saveCalibration();

  tft.fillScreen(COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_GOOD, COL_BG);
  tft.drawString("CALIBRATION SAVED", SCREEN_W / 2, SCREEN_H / 2, 4);
  delay(900);

  // The modal routine ran its own touch loop, so the state machine still
  // believes the finger from the last target is down. Clear it.
  consume();
  return true;
}
