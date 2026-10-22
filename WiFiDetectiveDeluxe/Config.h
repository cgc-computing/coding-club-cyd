// ============================================================================
//  Config.h -- Wi-Fi Detective Deluxe
//  Hardware pins, screen geometry, and the colour palette.
//
//  Board: ESP32-2432S028R ("Cheap Yellow Display", dual-USB revision)
//  All display values below match the club's hardware-verified configuration
//  in docs/CYD-SETUP.md. Do not change them casually.
// ============================================================================

#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Application identity
// ---------------------------------------------------------------------------

constexpr const char* APP_NAME    = "WI-FI DETECTIVE DELUXE";
constexpr const char* APP_VERSION = "0.1 (Phase 1)";

// ---------------------------------------------------------------------------
//  Display
//
//  The TFT pins themselves live in TFT_eSPI's User_Setup.h, not here -- the
//  library reads them at compile time. See README.md for the required setup.
//  Only the values the sketch itself needs are repeated below.
// ---------------------------------------------------------------------------

constexpr int      PIN_TFT_BACKLIGHT = 21;
constexpr uint8_t  TFT_ROTATION      = 1;      // 1 = landscape, 320x240
constexpr bool     TFT_INVERT        = true;   // required on our CYD revision

constexpr int16_t  SCREEN_W = 320;
constexpr int16_t  SCREEN_H = 240;

// Backlight PWM (LEDC). Requires ESP32 Arduino core 3.x.
constexpr int      BACKLIGHT_PWM_FREQ_HZ = 5000;
constexpr uint8_t  BACKLIGHT_PWM_BITS    = 8;      // duty 0..255
constexpr uint8_t  BACKLIGHT_DEFAULT     = 230;    // out of 255
constexpr uint8_t  BACKLIGHT_MINIMUM     = 25;     // never go fully dark

// ---------------------------------------------------------------------------
//  Touchscreen -- XPT2046 on its own SPI bus
//
//  IMPORTANT: on the CYD the touch controller does NOT share the display's
//  SPI bus. The display is on HSPI (GPIO 12/13/14/15); the touch panel is on
//  a separate set of pins driven here through VSPI. These are the documented
//  pin assignments for the ESP32-2432S028R. If your board behaves differently,
//  these are the first values to check.
// ---------------------------------------------------------------------------

constexpr int PIN_TOUCH_CLK  = 25;
constexpr int PIN_TOUCH_CS   = 33;
constexpr int PIN_TOUCH_MOSI = 32;
constexpr int PIN_TOUCH_MISO = 39;   // input-only pin, fine for MISO
constexpr int PIN_TOUCH_IRQ  = 36;   // input-only pin

// The XPT2046 library can use T_IRQ to avoid polling the SPI bus. That only
// works if T_IRQ is actually wired on your board -- and when it is not, touch
// silently never registers. Polling costs a few SPI reads per loop and always
// works, so it is the default. Set this to true only after confirming IRQ
// works on your hardware.
constexpr bool TOUCH_USE_IRQ = false;

constexpr uint32_t TOUCH_SPI_HZ = 2500000;   // XPT2046 is a slow part

// Touch behaviour
constexpr uint16_t TOUCH_DEBOUNCE_MS   = 40;   // ignore chatter after an edge
constexpr uint16_t TOUCH_REPRESS_MS    = 180;  // min gap between two taps
constexpr int16_t  TOUCH_TAP_SLOP_PX   = 18;   // max movement still called a tap
constexpr int16_t  TOUCH_SCROLL_MIN_PX = 10;   // movement before it is a drag

// Fallback calibration, used only until a real calibration is stored in NVS.
// These are typical raw XPT2046 ranges, not measured on your panel -- the
// sketch runs the calibration screen on first boot rather than trusting them.
constexpr bool     CAL_DEFAULT_SWAP_XY = true;
constexpr uint16_t CAL_DEFAULT_X_MIN   = 200;
constexpr uint16_t CAL_DEFAULT_X_MAX   = 3700;
constexpr uint16_t CAL_DEFAULT_Y_MIN   = 240;
constexpr uint16_t CAL_DEFAULT_Y_MAX   = 3800;

// ---------------------------------------------------------------------------
//  Non-volatile storage
// ---------------------------------------------------------------------------

constexpr const char* NVS_NAMESPACE = "wifidet";

// ---------------------------------------------------------------------------
//  Colour palette (RGB565) -- dark cyberpunk instrument
// ---------------------------------------------------------------------------

constexpr uint16_t COL_BG          = 0x0000;  // near black background
constexpr uint16_t COL_PANEL       = 0x10A2;  // very dark blue-grey panel
constexpr uint16_t COL_PANEL_HI    = 0x2124;  // panel, pressed/selected
constexpr uint16_t COL_GRID        = 0x2104;  // faint grid / rule lines

constexpr uint16_t COL_CYAN        = 0x07FF;  // primary text and accents
constexpr uint16_t COL_CYAN_DIM    = 0x03EF;  // inactive accent
constexpr uint16_t COL_TEXT        = 0xC618;  // light grey secondary text
constexpr uint16_t COL_TEXT_DIM    = 0x7BEF;  // dim grey tertiary text
constexpr uint16_t COL_WHITE       = 0xFFFF;

constexpr uint16_t COL_GOOD        = 0x07E0;  // strong signal  (green)
constexpr uint16_t COL_FAIR        = 0xFFE0;  // moderate       (yellow)
constexpr uint16_t COL_WARN        = 0xFD20;  // caution        (amber)
constexpr uint16_t COL_BAD         = 0xF800;  // weak signal    (red)

// ---------------------------------------------------------------------------
//  Layout
// ---------------------------------------------------------------------------

constexpr int16_t HEADER_H  = 28;
constexpr int16_t FOOTER_H  = 24;
constexpr int16_t CONTENT_Y = HEADER_H;
constexpr int16_t CONTENT_H = SCREEN_H - HEADER_H - FOOTER_H;   // 188 px
constexpr int16_t FOOTER_Y  = SCREEN_H - FOOTER_H;              // 216
