// FlipDigits.h -- the big digits for the flip cards, drawn from geometry.
//
// TFT_eSPI's large number fonts are not enabled in every setup the club
// uses, so this sketch does not depend on them. Each digit is described as a
// handful of thick strokes (straight lines and arcs). At startup we turn those
// strokes into a small grayscale picture per digit -- one byte per pixel,
// 0 = card background, 255 = solid digit, in-between = smooth edge.
//
// Nothing in this file touches the display, so it also compiles on a PC.

#pragma once

#include <math.h>
#include <stdint.h>

constexpr int DIGIT_W = 52;          // size of one digit picture, in pixels
constexpr int DIGIT_H = 80;

constexpr float STROKE_W    = 11.0f; // thickness of the digit strokes
constexpr float DIGIT_INSET = 6.0f;  // stroke centerlines live in a 40x68 box
                                     // starting this far in from the corner

// One stroke. If r == 0 it is a straight line from (x0,y0) to (x1,y1).
// Otherwise it is an arc: center (x0,y0), radius r, drawn clockwise on screen
// from angle x1 to angle y1, in degrees. 0 degrees points right, 90 points
// DOWN (screen y grows downward), 180 left, 270 up.
struct Stroke {
  float x0, y0, x1, y1, r;
};

constexpr int MAX_STROKES = 4;

struct DigitShape {
  int count;
  Stroke strokes[MAX_STROKES];
};

// Coordinates are stroke centerlines in a box 40 wide and 68 tall.
const DigitShape DIGIT_SHAPES[10] = {
  // 0: a tall oval -- two half circles joined by straight sides
  {4, {{20, 20, 180, 360, 20}, {20, 48, 0, 180, 20},
       {0, 20, 0, 48, 0}, {40, 20, 40, 48, 0}}},
  // 1: a stem with a small flag
  {2, {{24, 0, 24, 68, 0}, {9, 13, 24, 0, 0}}},
  // 2: a hook, a diagonal, and a foot
  {3, {{20, 20, 180, 400, 20}, {35.3f, 32.9f, 0, 68, 0}, {0, 68, 40, 68, 0}}},
  // 3: two bowls open to the left
  {2, {{19, 17, 200, 450, 17}, {19, 51, 270, 520, 17}}},
  // 4: a stem, a diagonal, and a crossbar
  {3, {{30, 0, 30, 68, 0}, {30, 0, 0, 46, 0}, {0, 46, 40, 46, 0}}},
  // 5: a flat top, a short stem, and a bowl
  {3, {{36, 0, 5, 0, 0}, {5, 0, 2.4f, 30.4f, 0}, {18, 46, 225, 510, 22}}},
  // 6: a full circle with a tail sweeping up to the right
  {2, {{20, 48, 0, 360, 20}, {3, 37, 27, 0, 0}}},
  // 7: a flat top and a diagonal
  {2, {{0, 0, 40, 0, 0}, {40, 0, 14, 68, 0}}},
  // 8: a small circle stacked on a larger one
  {2, {{20, 16, 0, 360, 16}, {20, 50, 0, 360, 18}}},
  // 9: a 6 turned upside down
  {2, {{20, 20, 0, 360, 20}, {37, 31, 13, 68, 0}}},
};

// Distance from point (px,py) to the line segment (ax,ay)-(bx,by).
inline float distToSegment(float px, float py,
                           float ax, float ay, float bx, float by) {
  float dx = bx - ax, dy = by - ay;
  float lenSq = dx * dx + dy * dy;
  float t = 0;
  if (lenSq > 0) {
    t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
  }
  float cx = ax + t * dx - px;
  float cy = ay + t * dy - py;
  return sqrtf(cx * cx + cy * cy);
}

// Distance from point (px,py) to one stroke's centerline. Arcs are measured
// as a chain of short straight pieces, which is plenty accurate at this size.
inline float distToStroke(float px, float py, const Stroke& s) {
  if (s.r == 0) {
    return distToSegment(px, py, s.x0, s.y0, s.x1, s.y1);
  }
  const float DEG = 3.14159265f / 180.0f;
  const float STEP = 12.0f;                       // degrees per piece
  int pieces = (int)ceilf((s.y1 - s.x1) / STEP);
  float best = 1e9f;
  float ax = s.x0 + s.r * cosf(s.x1 * DEG);
  float ay = s.y0 + s.r * sinf(s.x1 * DEG);
  for (int i = 1; i <= pieces; i++) {
    float angle = s.x1 + (s.y1 - s.x1) * i / pieces;
    float bx = s.x0 + s.r * cosf(angle * DEG);
    float by = s.y0 + s.r * sinf(angle * DEG);
    float d = distToSegment(px, py, ax, ay, bx, by);
    if (d < best) best = d;
    ax = bx;
    ay = by;
  }
  return best;
}

// Fill `out` (DIGIT_W * DIGIT_H bytes) with the picture of one digit.
// A pixel is solid if it is within half a stroke width of any centerline,
// and fades out over the last pixel so edges look smooth.
inline void renderDigit(int digit, uint8_t* out) {
  const DigitShape& shape = DIGIT_SHAPES[digit];
  for (int y = 0; y < DIGIT_H; y++) {
    for (int x = 0; x < DIGIT_W; x++) {
      float px = x + 0.5f - DIGIT_INSET;
      float py = y + 0.5f - DIGIT_INSET;
      float best = 1e9f;
      for (int i = 0; i < shape.count; i++) {
        float d = distToStroke(px, py, shape.strokes[i]);
        if (d < best) best = d;
      }
      float coverage = STROKE_W / 2 + 0.5f - best;
      if (coverage < 0) coverage = 0;
      if (coverage > 1) coverage = 1;
      out[y * DIGIT_W + x] = (uint8_t)(coverage * 255.0f + 0.5f);
    }
  }
}
