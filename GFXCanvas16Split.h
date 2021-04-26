#pragma once

#include "Adafruit_GFX.h"
#include "Adafruit_SPITFT.h"

//A GFX 16-bit canvas context for graphics.
// As opposed to the Adafruit GFXCanvas16, this one allocates the back buffer in X horizontal chunks
class GFXCanvas16Split : public Adafruit_GFX 
{
 public:
  GFXCanvas16Split(uint16_t w, uint16_t h, uint16_t chunkHeightBits = 5);
  ~GFXCanvas16Split();
  void      drawPixel(int16_t x, int16_t y, uint16_t color),
            fillScreen(uint16_t color);
  uint16_t* getBuffer(void);
  void blit(Adafruit_SPITFT& dst);
 private:
  uint16_t _chunkHeightBits = 0;
  uint16_t _chunkHeightMask = 0;
  uint16_t _chunkHeight = 0;
  uint16_t _chunkCount = 0;
  uint16_t** _chunks = nullptr;
};
