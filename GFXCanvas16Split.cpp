#include "GFXCanvas16Split.h"

/**************************************************************************/
/*!
   @brief    Instatiate a GFX 16-bit canvas context for graphics
   @param    w   Display width, in pixels
   @param    h   Display height, in pixels
*/
/**************************************************************************/
GFXCanvas16Split::GFXCanvas16Split(uint16_t w, uint16_t h, uint16_t chunkHeightBits) 
  : Adafruit_GFX(w, h)
  , _chunkHeightBits(chunkHeightBits)
{
  _chunkHeight = 1 << _chunkHeightBits;
  _chunkHeightMask = _chunkHeight - 1;
  
  _chunkCount = h / _chunkHeight;
  if (h % _chunkHeight != 0)
  {
    _chunkCount++;
  }    
  _chunks = new uint16_t*[_chunkCount];
  for (size_t i = 0; i < _chunkCount; i++)
  {
    _chunks[i] = new uint16_t[w * _chunkHeight];
  }
}

/**************************************************************************/
/*!
   @brief    Delete the canvas, free memory
*/
/**************************************************************************/
GFXCanvas16Split::~GFXCanvas16Split() 
{
  for (size_t i = 0; i < _chunkCount; i++)
  {
    delete[] _chunks[i];
  }
  delete[] _chunks;
}

/**************************************************************************/
/*!
   @brief    Draw a pixel to the canvas framebuffer
    @param   x   x coordinate
    @param   y   y coordinate
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFXCanvas16Split::drawPixel(int16_t x, int16_t y, uint16_t color) 
{
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

  int16_t cy = y >> _chunkHeightBits;
  int16_t coy = y & _chunkHeightMask;
  _chunks[cy][x + coy * _width] = color;
}

/**************************************************************************/
/*!
   @brief    Fill the framebuffer completely with one color
    @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFXCanvas16Split::fillScreen(uint16_t color) 
{
  uint8_t hi = color >> 8, lo = color & 0xFF;
  if(hi == lo) 
  {
    for (uint16_t i = 0; i < _chunkCount; i++)
    {
      memset(_chunks[i], lo, _width * _chunkHeight * sizeof(uint16_t));
    }
  } 
  else 
  {
    uint16_t sz = _width * _chunkHeight;
    for (uint16_t i = 0; i < _chunkCount; i++)
    {
      uint16_t* ptr = _chunks[i];
      int16_t n = (sz + 7) >> 3;
      switch (sz & 7) {
      case 0: do { *ptr++ = color;
      case 7:      *ptr++ = color;
      case 6:      *ptr++ = color;
      case 5:      *ptr++ = color;
      case 4:      *ptr++ = color;
      case 3:      *ptr++ = color;
      case 2:      *ptr++ = color;
      case 1:      *ptr++ = color;
        } while (--n > 0);
      }      
    }
  }
}

void GFXCanvas16Split::blit(Adafruit_SPITFT& dst)
{
  dst.startWrite();

  for (uint16_t i = 0; i < _chunkCount; i++)
  {
    uint16_t h = std::min<uint16_t>(_chunkHeight, _height - i * _chunkHeight);
    uint16_t sz = _width * h;
    dst.setAddrWindow(0, i * _chunkHeight, _width, h);
    dst.writePixels(_chunks[i], sz);
  }

  dst.endWrite();
}
