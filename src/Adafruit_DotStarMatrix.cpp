/*-------------------------------------------------------------------------
  Arduino library to control single and tiled matrices of APA102-based
  RGB LED devices such as displays assembled from DotStar strips, making
  them compatible with the Adafruit_GFX graphics library.  Requires both
  the Adafruit_DotStar and Adafruit_GFX libraries.

  Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  -------------------------------------------------------------------------
  This file is part of the Adafruit DotStarMatrix library.

  DotStarMatrix is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  DotStarMatrix is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with DotStarMatrix.  If not, see
  <http://www.gnu.org/licenses/>.
  -------------------------------------------------------------------------*/

#include <Adafruit_DotStar.h>
#include <Adafruit_DotStarMatrix.h>
#include "gamma.h"
#ifdef __AVR__
 #include <avr/pgmspace.h>
#elif defined(ESP8266)
 #include <pgmspace.h>
#else
 #ifndef pgm_read_byte
  #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
 #endif
#endif

#ifndef _swap_uint16_t
#define _swap_uint16_t(a, b) { uint16_t t = a; a = b; b = t; }
#endif

#if (PLATFORM_ID == 32)
// Constructor for single matrix w/hardware SPI/SPI1:
Adafruit_DotStarMatrix::Adafruit_DotStarMatrix(int w, int h,
  SPIClass& spi, uint8_t matrixType, uint8_t ledType) :
  Adafruit_GFX(w, h),
  Adafruit_DotStar(w * h, spi, ledType), type(matrixType),
  matrixWidth(w), matrixHeight(h), tilesX(0), tilesY(0), remapFn(NULL) { }

// Constructor for tiled matrices w/hardware SPI/SPI1:
Adafruit_DotStarMatrix::Adafruit_DotStarMatrix(uint8_t mW, uint8_t mH,
  uint8_t tX, uint8_t tY, SPIClass& spi, uint8_t matrixType, uint8_t ledType) :
  Adafruit_GFX(mW * tX, mH * tY), Adafruit_DotStar(mW * mH * tX * tY, spi,
  ledType), type(matrixType), matrixWidth(mW), matrixHeight(mH), tilesX(tX),
  tilesY(tY), remapFn(NULL) { }

#else // Argon, Boron, etc..
// Constructor for single matrix w/hardware SPI:
Adafruit_DotStarMatrix::Adafruit_DotStarMatrix(int w, int h,
  uint8_t matrixType, uint8_t ledType) : Adafruit_GFX(w, h),
  Adafruit_DotStar(w * h, ledType), type(matrixType), matrixWidth(w),
  matrixHeight(h), tilesX(0), tilesY(0), remapFn(NULL) { }

// Constructor for single matrix w/bitbang SPI:
Adafruit_DotStarMatrix::Adafruit_DotStarMatrix(int w, int h,
  uint8_t d, uint8_t c, uint8_t matrixType, uint8_t ledType) :
  Adafruit_GFX(w, h),
  Adafruit_DotStar(w * h, d, c, ledType), type(matrixType),
  matrixWidth(w), matrixHeight(h), tilesX(0), tilesY(0), remapFn(NULL) { }

// Constructor for tiled matrices w/hardware SPI:
Adafruit_DotStarMatrix::Adafruit_DotStarMatrix(uint8_t mW, uint8_t mH,
  uint8_t tX, uint8_t tY, uint8_t matrixType, uint8_t ledType) :
  Adafruit_GFX(mW * tX, mH * tY), Adafruit_DotStar(mW * mH * tX * tY,
  ledType), type(matrixType), matrixWidth(mW), matrixHeight(mH), tilesX(tX),
  tilesY(tY), remapFn(NULL) { }

// Constructor for tiled matrices w/bitbang SPI:
Adafruit_DotStarMatrix::Adafruit_DotStarMatrix(uint8_t mW, uint8_t mH,
  uint8_t tX, uint8_t tY, uint8_t d, uint8_t c, uint8_t matrixType,
  uint8_t ledType) : Adafruit_GFX(mW * tX, mH * tY),
  Adafruit_DotStar(mW * mH * tX * tY, d, c, ledType), type(matrixType),
  matrixWidth(mW), matrixHeight(mH), tilesX(tX), tilesY(tY), remapFn(NULL) { }
#endif // #if (PLATFORM_ID == 32)

// Expand 16-bit input color (Adafruit_GFX colorspace) to 24-bit (DotStar)
// (w/gamma adjustment)
static uint32_t expandColor(uint16_t color) {
  return ((uint32_t)pgm_read_byte(&gamma5[ color >> 11       ]) << 16) |
         ((uint32_t)pgm_read_byte(&gamma6[(color >> 5) & 0x3F]) <<  8) |
                    pgm_read_byte(&gamma5[ color       & 0x1F]);
}

// Downgrade 24-bit color to 16-bit (add reverse gamma lookup here?)
uint16_t Adafruit_DotStarMatrix::Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) |
                    (b         >> 3);
}

// Pass-through is a kludge that lets you override the current drawing
// color with a 'raw' RGB value that's issued directly to pixel(s),
// side-stepping the 16-bit color limitation of Adafruit_GFX.
// This is not without some limitations of its own -- for example, it
// won't work in conjunction with the background color feature when
// drawing text or bitmaps (you'll just get a solid rect of color),
// only 'transparent' text/bitmaps.  Also, no gamma correction.
// Remember to UNSET the passthrough color immediately when done with
// it (call with no value)!

// Pass raw color value to set/enable passthrough
void Adafruit_DotStarMatrix::setPassThruColor(uint32_t c) {
  passThruColor = c;
  passThruFlag  = true;
}

// Call without a value to reset (disable passthrough)
void Adafruit_DotStarMatrix::setPassThruColor(void) {
  passThruFlag = false;
}

void Adafruit_DotStarMatrix::drawPixel(int16_t x, int16_t y, uint16_t color) {

  if((x < 0) || (y < 0) || (x >= _width) || (y >= _height)) return;

  int16_t t;
  switch(rotation) {
   case 1:
    t = x;
    x = WIDTH  - 1 - y;
    y = t;
    break;
   case 2:
    x = WIDTH  - 1 - x;
    y = HEIGHT - 1 - y;
    break;
   case 3:
    t = x;
    x = y;
    y = HEIGHT - 1 - t;
    break;
  }

  int tileOffset = 0, pixelOffset;

  if(remapFn) { // Custom X/Y remapping function
    pixelOffset = (*remapFn)(x, y);
  } else {      // Standard single matrix or tiled matrices

    uint8_t  corner = type & DS_MATRIX_CORNER;
    uint16_t minor, major, majorScale;

    if(tilesX) { // Tiled display, multiple matrices
      uint16_t tile;

      minor = x / matrixWidth;            // Tile # X/Y; presume row major to
      major = y / matrixHeight,           // start (will swap later if needed)
      x     = x - (minor * matrixWidth);  // Pixel X/Y within tile
      y     = y - (major * matrixHeight); // (-* is less math than modulo)

      // Determine corner of entry, flip axes if needed
      if(type & DS_TILE_RIGHT)  minor = tilesX - 1 - minor;
      if(type & DS_TILE_BOTTOM) major = tilesY - 1 - major;

      // Determine actual major axis of tiling
      if((type & DS_TILE_AXIS) == DS_TILE_ROWS) {
        majorScale = tilesX;
      } else {
        _swap_uint16_t(major, minor);
        majorScale = tilesY;
      }

      // Determine tile number
      if((type & DS_TILE_SEQUENCE) == DS_TILE_PROGRESSIVE) {
        // All tiles in same order
        tile = major * majorScale + minor;
      } else {
        // Zigzag; alternate rows change direction.  On these rows,
        // this also flips the starting corner of the matrix for the
        // pixel math later.
        if(major & 1) {
          corner ^= DS_MATRIX_CORNER;
          tile = (major + 1) * majorScale - 1 - minor;
        } else {
          tile =  major      * majorScale     + minor;
        }
      }

      // Index of first pixel in tile
      tileOffset = tile * matrixWidth * matrixHeight;

    } // else no tiling (handle as single tile)

    // Find pixel number within tile
    minor = x; // Presume row major to start (will swap later if needed)
    major = y;

    // Determine corner of entry, flip axes if needed
    if(corner & DS_MATRIX_RIGHT)  minor = matrixWidth  - 1 - minor;
    if(corner & DS_MATRIX_BOTTOM) major = matrixHeight - 1 - major;

    // Determine actual major axis of matrix
    if((type & DS_MATRIX_AXIS) == DS_MATRIX_ROWS) {
      majorScale = matrixWidth;
    } else {
      _swap_uint16_t(major, minor);
      majorScale = matrixHeight;
    }

    // Determine pixel number within tile/matrix
    if((type & DS_MATRIX_SEQUENCE) == DS_MATRIX_PROGRESSIVE) {
      // All lines in same order
      pixelOffset = major * majorScale + minor;
    } else {
      // Zigzag; alternate rows change direction.
      if(major & 1) pixelOffset = (major + 1) * majorScale - 1 - minor;
      else          pixelOffset =  major      * majorScale     + minor;
    }
  }

  setPixelColor(tileOffset + pixelOffset,
    passThruFlag ? passThruColor : expandColor(color));
}

void Adafruit_DotStarMatrix::fillScreen(uint16_t color) {
  uint16_t i, n;
  uint32_t c;

  c = passThruFlag ? passThruColor : expandColor(color);
  n = numPixels();
  for(i=0; i<n; i++) setPixelColor(i, c);
}

void Adafruit_DotStarMatrix::setRemapFunction(uint16_t (*fn)(uint16_t, uint16_t)) {
  remapFn = fn;
}
