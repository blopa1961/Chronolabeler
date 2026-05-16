// These routines are part of Thermal_Printer Arduino library
// Copyright (c) 2020 BitBank Software, Inc.
// Written by Larry Bank (bitbank@pobox.com)
//
// Only the GFXfonts render routines and the line drawing routine are
// used. The line drawing routine could be simplified a lot, because I
// only draw a frame (hoizontal and vertical lines only)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// For a copy of the GNU General Public License see:
// <http://www.gnu.org/licenses/>.
//
#if !defined( _ADAFRUIT_GFX_H ) && !defined( _GFXFONT_H_ )
#define _GFXFONT_H_
typedef struct {
  uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
  uint8_t width;         ///< Bitmap dimensions in pixels
  uint8_t height;        ///< Bitmap dimensions in pixels
  uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
  int8_t xOffset;        ///< X dist from cursor pos to UL corner
  int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

/// Data stored for FONT AS A WHOLE
typedef struct {
  uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
  GFXglyph *glyph;  ///< Glyph array
  uint8_t first;    ///< ASCII extents (first char)
  uint8_t last;     ///< ASCII extents (last char)
  uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;
#endif // _ADAFRUIT_GFX_H

static uint8_t *pBackBuffer = NULL;
static int fontPitch;

// Return the measurements of a rectangle surrounding the given text string
// rendered in the given font
//
void getStringBox(GFXfont *pFont, char *szMsg, int *width, int *top, int *bottom)
{
int cx = 0;
int c, i = 0;
GFXglyph *pGlyph;
int miny, maxy;

   if (width == NULL || top == NULL || bottom == NULL || pFont == NULL || szMsg == NULL) return; // bad pointers
   miny = 100; maxy = 0;
   while (szMsg[i]) {
      c = szMsg[i++];
      if (c < pFont->first || c > pFont->last) // undefined character
         continue; // skip it
      c -= pFont->first; // first char of font defined
      pGlyph = &pFont->glyph[c];
      cx += pGlyph->xAdvance;
      if (pGlyph->yOffset < miny) miny = pGlyph->yOffset;
      if (pGlyph->height+pGlyph->yOffset > maxy) maxy = pGlyph->height+pGlyph->yOffset;
   }
   *width = cx;
   *top = miny;
   *bottom = maxy;
} /* tpGetStringBox() */

//
// Draw a string of characters in a custom font into the gfx buffer
//
int drawCustomText(GFXfont *pFont, int x, int y, char *szMsg)
{
int i, end_y, dx, dy, tx, ty, c, iBitOff;
uint8_t *s, *d, bits, ucMask, uc;
GFXglyph glyph, *pGlyph;


   if (pBackBuffer == NULL || pFont == NULL || x < 0 || y > myPrnCalc.bitmapHeight)
      return -1;
   pGlyph = &glyph;

   i = 0;
   while (szMsg[i] && x < myPrnCalc.bitmapWidth)
   {
      c = szMsg[i++];
      if (c < pFont->first || c > pFont->last) // undefined character
         continue; // skip it
      c -= pFont->first; // first char of font defined
      memcpy_P(&glyph, &pFont->glyph[c], sizeof(glyph));
      dx = x + pGlyph->xOffset; // offset from character UL to start drawing
      dy = y + pGlyph->yOffset;
      s = pFont->bitmap + pGlyph->bitmapOffset; // start of bitmap data
      // Bitmap drawing loop. Image is MSB first and each pixel is packed next
      // to the next (continuing on to the next character line)
      iBitOff = 0; // bitmap offset (in bits)
      bits = uc = 0; // bits left in this font byte
      end_y = dy + pGlyph->height;
      if (dy < 0) { // skip these lines
          iBitOff += (pGlyph->width * (-dy));
          dy = 0;
      }
//      for (ty=dy; ty<=end_y && ty < myPrnCalc.bitmapHeight; ty++) {
// blopa1961 -> fixed this Bug
      for (ty=dy; ty<end_y && ty < myPrnCalc.bitmapHeight; ty++) {
         d = &pBackBuffer[ty * fontPitch]; // internal buffer dest
         for (tx=0; tx<pGlyph->width; tx++) {
            if (uc == 0) { // need to read more font data
               tx += bits; // skip any remaining 0 bits
               uc = pgm_read_byte(&s[iBitOff>>3]); // get more font bitmap data
               bits = 8 - (iBitOff & 7); // we might not be on a byte boundary
               iBitOff += bits; // because of a clipped line
               uc <<= (8-bits);
               if (tx >= pGlyph->width) {
                  while(tx >= pGlyph->width) { // rolls into next line(s)
                     tx -= pGlyph->width;
                     ty++;
                  }
                  if (ty >= end_y) { // we're past the end
                     tx = pGlyph->width;
                     continue; // exit this character cleanly
                  }
                  d = &pBackBuffer[ty * fontPitch];
               }
            } // if we ran out of bits
            if (uc & 0x80) { // set pixel
               ucMask = 0x80 >> ((dx+tx) & 7);
//               d[(dx+tx)>>3] |= ucMask;
// blopa1961 -> changed the default bitwise OR by XOR, which toggles the background despite of current color
// this way, the background can be WHITE or BLACK and the color will be toggled
               d[(dx+tx)>>3] ^= ucMask;
            }
            bits--; // next bit
            uc <<= 1;
         } // for x
      } // for y
      x += pGlyph->xAdvance; // width of this character
   } // while drawing characters
   return 0;
} /* tpDrawCustomText() */
//

//
// Draw a line between 2 points
//
void drawLine(int x1, int y1, int x2, int y2, uint8_t ucColor)
{
  int temp;
  int dx = x2 - x1;
  int dy = y2 - y1;
  int error;
  uint8_t *p, mask;
  int xinc, yinc;

  if (x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0 || x1 >= myPrnCalc.bitmapWidth || x2 >= myPrnCalc.bitmapWidth || y1 >= myPrnCalc.bitmapHeight || y2 >= myPrnCalc.bitmapHeight)
     return;

  if(abs(dx) > abs(dy)) {
    // X major case
    if(x2 < x1) {
      dx = -dx;
      temp = x1;
      x1 = x2;
      x2 = temp;
      temp = y1;
      y1 = y2;
      y2 = temp;
    }

    dy = (y2 - y1);
    error = dx >> 1;
    yinc = 1;
    if (dy < 0)
    {
      dy = -dy;
      yinc = -1;
    }
    p = &pBackBuffer[(y1 * fontPitch) + (x1 >> 3)]; // point to current spot in back buffer
    mask = 0x80 >> (x1 & 7); // current bit offset
    for(; x1 <= x2; x1++) {
      if (ucColor)
        *p |= mask; // set pixel and increment x pointer
      else
        *p &= ~mask;
      mask >>= 1;
      if (mask == 0) {
         mask = 0x80;
         p++;
      }
      error -= dy;
      if (error < 0)
      {
        error += dx;
        if (yinc > 0)
           p += fontPitch;
        else
           p -= fontPitch;
      }
    } // for x1
  }
  else {
    // Y major case
    if(y1 > y2) {
      dy = -dy;
      temp = x1;
      x1 = x2;
      x2 = temp;
      temp = y1;
      y1 = y2;
      y2 = temp;
    }

    p = &pBackBuffer[(y1 * fontPitch) + (x1 >> 3)]; // point to current spot in back buffer
    mask = 0x80 >> (x1 & 7); // current bit offset
    dx = (x2 - x1);
    error = dy >> 1;
    xinc = 1;
    if (dx < 0)
    {
      dx = -dx;
      xinc = -1;
    }
    for(; y1 <= y2; y1++) {
      if (ucColor)
         *p |= mask; // set the pixel
      else
         *p &= ~mask;
      p += fontPitch; // y++
      error -= dx;
      if (error < 0)
      {
        error += dy;
        x1 += xinc;
        if (xinc > 0)
        {
          mask >>= 1;
          if (mask == 0) // change the byte
          {
             p++;
             mask = 0x80;
          }
        } // positive delta x
        else // negative delta x
        {
          mask <<= 1;
          if (mask == 0)
          {
             p--;
             mask = 1;
          }
        }
      }
    } // for y
  } // y major case
} /* tpDrawLine() */
