#pragma once

#include "Adafruit_GFX.h"
#include "Adafruit_SPITFT.h"

class JLMBackBuffer : public Adafruit_GFX 
{
public:
  JLMBackBuffer(uint16_t w, uint16_t h, uint16_t cellWBits, uint16_t cellHBits);
  ~JLMBackBuffer();

  void setOpacity(uint8_t opacity);
  void setClipRect(int16_t x, int16_t y, int16_t w, int16_t h);

  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void fillScreen(uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

  void drawAALine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
  void drawAACircle(int16_t x, int16_t y, int16_t r, uint16_t color);

  void blit(Adafruit_SPITFT& dst);

private:
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
  void _writePixel(int16_t x, int16_t y, uint16_t color, uint8_t alpha);
  void _writePixelNC(int16_t x, int16_t y, uint16_t color, uint8_t alpha);
  void _fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void _drawSingleCellHLine(int16_t x, int16_t y, int16_t w, uint16_t color, uint32_t c32, uint8_t a32);
  void _drawSingleCellVLine(int16_t x, int16_t y, int16_t h, uint16_t color, uint32_t c32, uint8_t a32);
  void _drawVLineClipped(int16_t x, int16_t y, int16_t w, uint16_t color);
  void _drawHLineClipped(int16_t x, int16_t y, int16_t w, uint16_t color);

  uint16_t _cellWBits = 0;
  uint16_t _cellHBits = 0;
  uint16_t _cellWMask = 0;
  uint16_t _cellHMask = 0;
  uint16_t _cellW = 0;
  uint16_t _cellH = 0;
  uint16_t _cellCountX = 0;
  uint16_t _cellCountY = 0;

  typedef uint32_t CellHash;

  struct MemBlock
  {
    uint16_t* data; //allocated from the heap. Owned
  };

  struct Cell
  {
    CellHash prevHash = 0;
    uint16_t* data = nullptr; //data from a mem block. Not owned
  };

  inline void acquireCell(Cell& cell);
  inline void fillCell(Cell& cell, uint16_t color, uint8_t opacity);

  CellHash computeCellHash(Cell& cell) const;

  uint8_t m_opacityLUT[256];
  std::vector<MemBlock> _memBlocks;
  size_t _crtMemBlock = 0;
  size_t _crtMemBlockOffset = 0;
  size_t _cellCount = 0;
  Cell* _cells = nullptr;
  Cell _clearCell;
  CellHash _clearCellHash = 0;
  uint16_t _clearColor = 0;
  uint8_t _opacity = 0xFF;
  int16_t _clipX = 0;
  int16_t _clipY = 0;
  int16_t _clipX2 = 1;
  int16_t _clipY2 = 1;
  size_t _cellDataSize = 0;
  size_t _memBlockSize = 0;
};
