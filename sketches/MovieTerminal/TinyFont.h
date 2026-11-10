// TinyFont.h -- a very small font: every character is 3 pixels wide and
// 5 pixels tall, drawn in a 4 x 6 cell (one blank column and one blank row
// for spacing). That fits 80 characters across the 320-pixel screen.
//
// Each character is five rows of three pixels. G() packs those 15 pixels
// into one number. Read each row as binary: 1 = pixel on, 0 = pixel off.
// For example the letter T is
//
//     111      ###
//     010       #
//     010       #
//     010       #
//     010       #

#pragma once

#include <stdint.h>

constexpr int CELL_W = 4;     // one character cell, including spacing
constexpr int CELL_H = 6;
constexpr int GLYPH_W = 3;    // the part of the cell that holds pixels
constexpr int GLYPH_H = 5;

constexpr uint16_t G(int a, int b, int c, int d, int e) {
  return (uint16_t)((a << 12) | (b << 9) | (c << 6) | (d << 3) | e);
}

// Characters ' ' (32) through '~' (126), in ASCII order.
const uint16_t TINY_FONT[95] = {
  G(0b000, 0b000, 0b000, 0b000, 0b000),  // space
  G(0b010, 0b010, 0b010, 0b000, 0b010),  // !
  G(0b101, 0b101, 0b000, 0b000, 0b000),  // "
  G(0b101, 0b111, 0b101, 0b111, 0b101),  // #
  G(0b011, 0b110, 0b010, 0b011, 0b110),  // $
  G(0b101, 0b001, 0b010, 0b100, 0b101),  // %
  G(0b010, 0b101, 0b010, 0b101, 0b011),  // &
  G(0b010, 0b010, 0b000, 0b000, 0b000),  // '
  G(0b001, 0b010, 0b010, 0b010, 0b001),  // (
  G(0b100, 0b010, 0b010, 0b010, 0b100),  // )
  G(0b000, 0b101, 0b010, 0b101, 0b000),  // *
  G(0b000, 0b010, 0b111, 0b010, 0b000),  // +
  G(0b000, 0b000, 0b000, 0b010, 0b100),  // ,
  G(0b000, 0b000, 0b111, 0b000, 0b000),  // -
  G(0b000, 0b000, 0b000, 0b000, 0b010),  // .
  G(0b001, 0b001, 0b010, 0b100, 0b100),  // /
  G(0b111, 0b101, 0b101, 0b101, 0b111),  // 0
  G(0b010, 0b110, 0b010, 0b010, 0b111),  // 1
  G(0b111, 0b001, 0b111, 0b100, 0b111),  // 2
  G(0b111, 0b001, 0b011, 0b001, 0b111),  // 3
  G(0b101, 0b101, 0b111, 0b001, 0b001),  // 4
  G(0b111, 0b100, 0b111, 0b001, 0b111),  // 5
  G(0b111, 0b100, 0b111, 0b101, 0b111),  // 6
  G(0b111, 0b001, 0b001, 0b010, 0b010),  // 7
  G(0b111, 0b101, 0b111, 0b101, 0b111),  // 8
  G(0b111, 0b101, 0b111, 0b001, 0b111),  // 9
  G(0b000, 0b010, 0b000, 0b010, 0b000),  // :
  G(0b000, 0b010, 0b000, 0b010, 0b100),  // ;
  G(0b001, 0b010, 0b100, 0b010, 0b001),  // <
  G(0b000, 0b111, 0b000, 0b111, 0b000),  // =
  G(0b100, 0b010, 0b001, 0b010, 0b100),  // >
  G(0b111, 0b001, 0b011, 0b000, 0b010),  // ?
  G(0b111, 0b101, 0b101, 0b100, 0b111),  // @
  G(0b010, 0b101, 0b111, 0b101, 0b101),  // A
  G(0b110, 0b101, 0b110, 0b101, 0b110),  // B
  G(0b011, 0b100, 0b100, 0b100, 0b011),  // C
  G(0b110, 0b101, 0b101, 0b101, 0b110),  // D
  G(0b111, 0b100, 0b110, 0b100, 0b111),  // E
  G(0b111, 0b100, 0b110, 0b100, 0b100),  // F
  G(0b011, 0b100, 0b101, 0b101, 0b011),  // G
  G(0b101, 0b101, 0b111, 0b101, 0b101),  // H
  G(0b111, 0b010, 0b010, 0b010, 0b111),  // I
  G(0b001, 0b001, 0b001, 0b101, 0b010),  // J
  G(0b101, 0b101, 0b110, 0b101, 0b101),  // K
  G(0b100, 0b100, 0b100, 0b100, 0b111),  // L
  G(0b101, 0b111, 0b111, 0b101, 0b101),  // M
  G(0b110, 0b101, 0b101, 0b101, 0b101),  // N
  G(0b010, 0b101, 0b101, 0b101, 0b010),  // O
  G(0b110, 0b101, 0b110, 0b100, 0b100),  // P
  G(0b010, 0b101, 0b101, 0b111, 0b011),  // Q
  G(0b110, 0b101, 0b110, 0b101, 0b101),  // R
  G(0b011, 0b100, 0b010, 0b001, 0b110),  // S
  G(0b111, 0b010, 0b010, 0b010, 0b010),  // T
  G(0b101, 0b101, 0b101, 0b101, 0b111),  // U
  G(0b101, 0b101, 0b101, 0b101, 0b010),  // V
  G(0b101, 0b101, 0b111, 0b111, 0b101),  // W
  G(0b101, 0b101, 0b010, 0b101, 0b101),  // X
  G(0b101, 0b101, 0b010, 0b010, 0b010),  // Y
  G(0b111, 0b001, 0b010, 0b100, 0b111),  // Z
  G(0b011, 0b010, 0b010, 0b010, 0b011),  // [
  G(0b100, 0b100, 0b010, 0b001, 0b001),  // backslash
  G(0b110, 0b010, 0b010, 0b010, 0b110),  // ]
  G(0b010, 0b101, 0b000, 0b000, 0b000),  // ^
  G(0b000, 0b000, 0b000, 0b000, 0b111),  // _
  G(0b100, 0b010, 0b000, 0b000, 0b000),  // `
  G(0b000, 0b011, 0b101, 0b101, 0b011),  // a
  G(0b100, 0b110, 0b101, 0b101, 0b110),  // b
  G(0b000, 0b011, 0b100, 0b100, 0b011),  // c
  G(0b001, 0b011, 0b101, 0b101, 0b011),  // d
  G(0b000, 0b010, 0b111, 0b100, 0b011),  // e
  G(0b001, 0b010, 0b111, 0b010, 0b010),  // f
  G(0b000, 0b011, 0b101, 0b011, 0b110),  // g
  G(0b100, 0b110, 0b101, 0b101, 0b101),  // h
  G(0b010, 0b000, 0b010, 0b010, 0b010),  // i
  G(0b001, 0b000, 0b001, 0b101, 0b010),  // j
  G(0b100, 0b101, 0b110, 0b101, 0b101),  // k
  G(0b110, 0b010, 0b010, 0b010, 0b111),  // l
  G(0b000, 0b111, 0b111, 0b101, 0b101),  // m
  G(0b000, 0b110, 0b101, 0b101, 0b101),  // n
  G(0b000, 0b010, 0b101, 0b101, 0b010),  // o
  G(0b000, 0b110, 0b101, 0b110, 0b100),  // p
  G(0b000, 0b011, 0b101, 0b011, 0b001),  // q
  G(0b000, 0b101, 0b110, 0b100, 0b100),  // r
  G(0b000, 0b011, 0b110, 0b011, 0b110),  // s
  G(0b010, 0b111, 0b010, 0b010, 0b001),  // t
  G(0b000, 0b101, 0b101, 0b101, 0b011),  // u
  G(0b000, 0b101, 0b101, 0b101, 0b010),  // v
  G(0b000, 0b101, 0b101, 0b111, 0b111),  // w
  G(0b000, 0b101, 0b010, 0b010, 0b101),  // x
  G(0b000, 0b101, 0b011, 0b001, 0b110),  // y
  G(0b000, 0b111, 0b010, 0b100, 0b111),  // z
  G(0b011, 0b010, 0b110, 0b010, 0b011),  // {
  G(0b010, 0b010, 0b010, 0b010, 0b010),  // |
  G(0b110, 0b010, 0b011, 0b010, 0b110),  // }
  G(0b000, 0b011, 0b110, 0b000, 0b000),  // ~
};

// The three pixels of one row of a character, as a number 0..7.
// `row` is 0 at the top. Anything that is not a printable character,
// and the blank spacing row under each glyph, comes back as 0.
inline int glyphRow(char c, int row) {
  if (c < 32 || c > 126 || row >= GLYPH_H) return 0;
  return (TINY_FONT[c - 32] >> ((GLYPH_H - 1 - row) * GLYPH_W)) & 0b111;
}
