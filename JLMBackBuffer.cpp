#include "JLMBackBuffer.h"

//generated with wolfram alpha with a 2.2 gamma curge and rounding
//IntegerPart[pow(x/255, 0.4545)*31 + 0.5] for 0 <= x <= 255 
static uint8_t k_gammaAlpha[256] = {0, 2, 3, 4, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 31, 31, 31, 31};
                             
using WritePixelFN = void(JLMBackBuffer::*)(int16_t x, int16_t y, uint16_t color, uint8_t alpha);
     

JLMBackBuffer::JLMBackBuffer(uint16_t w, uint16_t h, uint16_t cellWBits, uint16_t cellHBits)
	: Adafruit_GFX(w, h)
	, _cellWBits(cellWBits)
	, _cellHBits(cellHBits)
	, _cellWMask((1 << cellWBits) - 1)
	, _cellHMask((1 << cellHBits) - 1)
	, _cellW(1 << cellWBits)
	, _cellH(1 << cellHBits)
	, _clipX2(w - 1)
	, _clipY2(h - 1)
{
	_cellCountX = w / _cellW;
	if (w % _cellW != 0)
	{
		_cellCountX++;
	}
	_cellCountY = h / _cellH;
	if (h % _cellH != 0)
	{
		_cellCountY++;
	}

  _cellDataSize = _cellW * _cellH;
  _cellCount = _cellCountX * _cellCountY;
	_cells = new Cell[_cellCount];
	_clearCell.data = new uint16_t[_cellDataSize];
  fillCell(_clearCell, _clearColor, 0xFF);
  _clearCellHash = computeCellHash(_clearCell);

  _memBlockSize = std::max<size_t>(_cellDataSize, 1024);
  _memBlocks.reserve(32);

  memcpy(m_opacityLUT, k_gammaAlpha, 256);
}

JLMBackBuffer::~JLMBackBuffer()
{
  delete[] _cells;
	delete[] _clearCell.data;
 
	for (MemBlock& block: _memBlocks)
	{
		delete block.data;
	}
}

void JLMBackBuffer::setOpacity(uint8_t opacity)
{
	if (_opacity != opacity)
  {
    _opacity = opacity;
    for (size_t i = 0; i < 256; i++)
    {
      m_opacityLUT[i] = (uint8_t)(((uint32_t)k_gammaAlpha[i] * (uint32_t)opacity) / 255);
    }
  }
} 

void JLMBackBuffer::setClipRect(int16_t x, int16_t y, int16_t w, int16_t h)
{
	_clipX = x >= 0 ? x : 0;
	_clipY = y >= 0 ? y : 0;
	_clipX2 = w >= 1 ? _clipX + w - 1 : _clipX;
	_clipY2 = h >= 1 ? _clipY + h - 1 : _clipY;
}

// inline uint16_t blend(uint16_t c0, uint16_t c1, uint8_t a)
// {
// 	//565
// 	//rrrrrggggggbbbbb
// 	//rrrrr-----gggggg------bbbbb-----
// }

inline uint16_t blend(uint32_t bg, uint32_t fg, uint8_t alpha)
{
    // Alpha converted from [0..255] to [0..31]
    //alpha = (alpha + 4) >> 3;
    alpha = k_gammaAlpha[alpha];

    // Converts  0000000000000000rrrrrggggggbbbbb
    //     into  00000gggggg00000rrrrr000000bbbbb
    // with mask 00000111111000001111100000011111
    // This is useful because it makes space for a parallel fixed-point multiply
    bg = (bg | (bg << 16)) & 0b00000111111000001111100000011111;
    fg = (fg | (fg << 16)) & 0b00000111111000001111100000011111;

    // This implements the linear interpolation formula: result = bg * (1.0 - alpha) + fg * alpha
    // This can be factorized into: result = bg + (fg - bg) * alpha
    // alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
    uint32_t result = (fg - bg) * alpha; // parallel fixed-point multiply of all components
    result >>= 5;
    result += bg;
    result &= 0b00000111111000001111100000011111; // mask out fractional parts
    return (int16_t)((result >> 16) | result); // contract result
}

inline uint8_t encodeAlpha(uint8_t alpha)
{
    return k_gammaAlpha[alpha];//(alpha + 4) >> 3;
}
inline uint32_t encodeColor(uint16_t color)
{
    return (color | (color << 16)) & 0b00000111111000001111100000011111;
}

void JLMBackBuffer::drawPixel(int16_t x, int16_t y, uint16_t color)
{
	writePixel(x, y, color);
}
void JLMBackBuffer::_writePixel(int16_t x, int16_t y, uint16_t color, uint8_t alpha)
{
	if (x < _clipX || y < _clipY || x > _clipX2 || y > _clipY2) 
	{
		return;
	}
  _writePixelNC(x, y, color, alpha);
}
void JLMBackBuffer::_writePixelNC(int16_t x, int16_t y, uint16_t color, uint8_t alpha)
{
  int16_t cx = x >> _cellWBits;
  int16_t cy = y >> _cellHBits;
  int16_t cox = x & _cellWMask;
  int16_t coy = y & _cellHMask;

  uint16_t cellIndex = cy * _cellCountX + cx;
  Cell& cell = _cells[cellIndex];
  if (!cell.data)
  {
    acquireCell(cell);
  }

  uint16_t& dst = cell.data[(coy << _cellWBits) + cox];
  uint8_t a32 = m_opacityLUT[alpha];
  if (a32 >= 31)
  {
    dst = color;
  }
  else
  {
    // Converts  0000000000000000rrrrrggggggbbbbb
    //     into  00000gggggg00000rrrrr000000bbbbb
    // with mask 00000111111000001111100000011111
    // This is useful because it makes space for a parallel fixed-point multiply
    uint32_t bg = (dst | (dst << 16)) & 0b00000111111000001111100000011111;
    uint32_t fg = (color | (color << 16)) & 0b00000111111000001111100000011111;

    // This implements the linear interpolation formula: result = bg * (1.0 - alpha) + fg * alpha
    // This can be factorized into: result = bg + (fg - bg) * alpha
    // alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
    uint32_t result = (fg - bg) * a32; // parallel fixed-point multiply of all components
    result >>= 5;
    result += bg;
    result &= 0b00000111111000001111100000011111; // mask out fractional parts
    dst = (int16_t)((result >> 16) | result); // contract result
  }
}

void JLMBackBuffer::writePixel(int16_t x, int16_t y, uint16_t color)
{
  if (x < _clipX || y < _clipY || x > _clipX2 || y > _clipY2) 
  {
    return;
  }
  
  if (_opacity <= 223)
  {
    _writePixelNC(x, y, color, 0xFF);
    return;
  }
  
  int16_t cx = x >> _cellWBits;
  int16_t cy = y >> _cellHBits;
  int16_t cox = x & _cellWMask;
  int16_t coy = y & _cellHMask;

  uint16_t cellIndex = cy * _cellCountX + cx;
  Cell& cell = _cells[cellIndex];
  if (!cell.data)
  {
    acquireCell(cell);
  }

  cell.data[(coy << _cellWBits) + cox] = color;
}

void JLMBackBuffer::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if (x > _clipX2 || y > _clipY2)
	{
		return;
	}
  int16_t x2 = x + w - 1;
	int16_t y2 = y + h - 1;
  if (x2 < _clipX || y2 < _clipY)
	{
		return;
	}

  // Clip left/top
  if (x < _clipX) 
	{
    x = _clipX;
    w = x2 - x + 1;
  }
  if (y < _clipY) 
	{
    y = _clipY;
    h = y2 - y + 1;
  }

  // Clip right/bottom
  if (x2 > _clipX2)
	{
		w = _clipX2 - x + 1;
	}
  if (y2 >= _clipY2) 
	{
		h = _clipY2 - y + 1;
	}

	if (w < _cellW || h < _cellH) //rect too small, go for the slow routine
	{
		_fillRect(x, y, w, h, color);
	}
	else
	{
		//these are the areas. 0, 1, 2, 3 are partial cells, 4 are full cells
		//  000000000000
		//  144444444442
		//  144444444442
		//  133333333332

		//Area 0
		if ((y & _cellHMask) != 0)
		{
			int16_t hh = _cellH - (y & _cellHMask);
      for (int16_t i = 0; i < hh; i++)
      {
        drawFastHLine(x, y + i, w, color);
      }
			y += hh;
			h -= hh;
		}
		//Area 1
		if ((x & _cellWMask) != 0)
		{
			int16_t ww = _cellW - (x & _cellWMask);
      for (int16_t i = 0; i < ww; i++)
      {
        drawFastVLine(x + i, y, h, color);
      }
			x += ww;
			w -= ww;
		}
		//Area 2
		if (((x + w) & _cellWMask) != 0)
		{
			int16_t ww = (x + w) & _cellWMask;
      int16_t xx = x + w - ww;
      for (int16_t i = 0; i < ww; i++)
      {
        drawFastVLine(xx + i, y, h, color);
      }
			w -= ww;
		}
		//Area 3
		if (((y + h) & _cellHMask) != 0)
		{
			int16_t hh = (y + h) & _cellHMask;
      int16_t yy = y + h - hh;
      for (int16_t i = 0; i < hh; i++)
      {
        drawFastHLine(x, yy + i, w, color);
      }
			h -= hh;
		}

		//Area 4 - full cells only
		int16_t cx = x >> _cellWBits;
		int16_t cy = y >> _cellHBits;
		int16_t cw = w >> _cellWBits;
		int16_t ch = h >> _cellHBits;
		for (int16_t j = 0; j < ch; j++)
		{
			uint16_t cellIndex = (cy + j) * _cellCountX;
			for (int16_t i = 0; i < cw; i++)
			{
				Cell& cell = _cells[cellIndex + cx + i];
        if (!cell.data)
        {
          acquireCell(cell);
        }        
				fillCell(cell, color, _opacity);
			}
		}
	}
}
void JLMBackBuffer::_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
	for (int16_t j = 0; j < h; j++)
	{
    drawFastHLine(x, y + j, w, color);
	}
}

void JLMBackBuffer::_drawSingleCellHLine(int16_t x, int16_t y, int16_t w, uint16_t color, uint32_t c32, uint8_t a32)
{
  int16_t yy = y;
  int16_t cy = yy >> _cellHBits; //cell Y
  int16_t coy = yy & _cellHMask; //cell Offset Y
  uint16_t cp = coy << _cellWBits;

  int16_t cx = x >> _cellWBits; //cell X
  int16_t cox = x & _cellWMask; //cell Offset X
  Cell& cell = _cells[cy * _cellCountX + cx];
  if (!cell.data)
  {
    acquireCell(cell);
  }        

  uint16_t* ptr = cell.data + cp + cox;
  if (a32 >= 31)
  {
    int16_t n = (w + 7) >> 3;
    switch (w & 7) {
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
  else
  {
    for (int16_t i = 0; i < w; i++)
    {
      uint16_t dst = *ptr;
      uint32_t bg = (dst | (dst << 16)) & 0b00000111111000001111100000011111;
      uint32_t result = (((c32 - bg) * a32) >> 5) + bg;
      result &= 0b00000111111000001111100000011111; // mask out fractional parts
      *ptr++ = (int16_t)((result >> 16) | result); // contract result      
    }
  }
}

void JLMBackBuffer::_drawSingleCellVLine(int16_t x, int16_t y, int16_t h, uint16_t color, uint32_t c32, uint8_t a32)
{
  int16_t xx = x;
  int16_t cx = xx >> _cellWBits; //cell X
  int16_t cox = xx & _cellWMask; //cell Offset X

  int16_t cy = y >> _cellHBits; //cell Y
  Cell& cell = _cells[cy * _cellCountX + cx];
  if (!cell.data)
  {
    acquireCell(cell);
  }        
  uint16_t* ptr = cell.data + ((y & _cellHMask) << _cellWBits) + cox;
  if (a32 >= 31)
  {
    int16_t n = (h + 7) >> 3;
    switch (h & 7) {
    case 0: do { *ptr = color; ptr += _cellW;
    case 7:      *ptr = color; ptr += _cellW;
    case 6:      *ptr = color; ptr += _cellW;
    case 5:      *ptr = color; ptr += _cellW;
    case 4:      *ptr = color; ptr += _cellW;
    case 3:      *ptr = color; ptr += _cellW;
    case 2:      *ptr = color; ptr += _cellW;
    case 1:      *ptr = color; ptr += _cellW;
      } while (--n > 0);
    }    
  }
  else
  {
    for (int16_t j = 0; j < h; j++)
    {
      uint16_t dst = *ptr;
      uint32_t bg = (dst | (dst << 16)) & 0b00000111111000001111100000011111;
      uint32_t result = (((c32 - bg) * a32) >> 5) + bg;
      result &= 0b00000111111000001111100000011111; // mask out fractional parts
      *ptr =  (int16_t)((result >> 16) | result); // contract result      
      ptr += _cellW;
    }
  }
}

void JLMBackBuffer::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  if (x < _clipX || x > _clipX2 || y > _clipY2)
  {
    return;
  }
  int16_t y2 = y + h - 1;
  if (y2 < _clipY)
  {
    return;
  }

  // Clip top
  if (y < _clipY) 
  {
      y = _clipY;
      h = y2 - y + 1;
  }

  // Clip bottom
  if (y2 >= _clipY2) 
  {
    h = _clipY2 - y + 1;
  }

  _drawVLineClipped(x, y, h, color);
}

void JLMBackBuffer::_drawVLineClipped(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  uint8_t a32 = encodeAlpha(_opacity);
  uint32_t c32 = encodeColor(color);

  //these are the areas. 0, 1 are partial cells, 2 are full cells
  //  0
  //  2
  //  2
  //  1

  //Area 0
  if ((y & _cellHMask) != 0) //area 0 is not empty
  {
    int16_t hh = std::min<int16_t>(_cellH - (y & _cellHMask), h); // how many pixels in area
    _drawSingleCellVLine(x, y, hh, color, c32, a32);
    y += hh;
    h -= hh;
  }

  if (h == 0)
  {
    return;
  }
    
  //Area 1
  if (((y + h) & _cellHMask) != 0)
  {
    int16_t hh = (y + h) & _cellHMask; // how many pixels in area
    int16_t yy = y + h - hh; //y in area
    _drawSingleCellVLine(x, yy, hh, color, c32, a32);
    h -= hh;
  }

  if (h == 0)
  {
    return;
  }
  
  //Area 2 - full cells only
  int16_t ch = h >> _cellHBits;
  for (int16_t j = 0; j < ch; j++)
  {
    _drawSingleCellVLine(x, y + j * _cellH, _cellH, color, c32, a32);
  }
}

void JLMBackBuffer::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  if (x > _clipX2 || y > _clipY2 || y < _clipY)
  {
    return;
  }
  int16_t x2 = x + w - 1;
  if (x2 < _clipX)
  {
    return;
  }

  // Clip left
  if (x < _clipX) 
  {
    x = _clipX;
    w = x2 - x + 1;
  }

  // Clip right
  if (x2 > _clipX2)
  {
    w = _clipX2 - x + 1;
  }

  _drawHLineClipped(x, y, w, color);
}

void JLMBackBuffer::_drawHLineClipped(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  uint8_t a32 = encodeAlpha(_opacity);
  uint32_t c32 = encodeColor(color);

  //these are the areas. 0, 1 are partial cells, 2 are full cells
  //  0221

  //Area 0
  if ((x & _cellWMask) != 0) //area 0 is not empty
  {
    int16_t ww = std::min<int16_t>(_cellW - (x & _cellWMask), w); // how many pixels in area
    _drawSingleCellHLine(x, y, ww, color, c32, a32);
    x += ww;
    w -= ww;
  }

  if (w == 0)
  {
    return;
  }
    
  //Area 1
  if (((x + w) & _cellWMask) != 0)
  {
    int16_t ww = (x + w) & _cellWMask; // how many pixels in area
    int16_t xx = x + w - ww; //y in area
    _drawSingleCellHLine(xx, y, ww, color, c32, a32);
    w -= ww;
  }

  if (w == 0)
  {
    return;
  }
  
  //Area 2 - full cells only
  int16_t cw = w >> _cellWBits;
  for (int16_t i = 0; i < cw; i++)
  {
    _drawSingleCellHLine(x + (i << _cellWBits), y, _cellW, color, c32, a32);
  }
}

void JLMBackBuffer::fillScreen(uint16_t color)
{
	fillCell(_clearCell, color, 0xFF);
	_clearCellHash = computeCellHash(_clearCell);
	_clearColor = color;

	for (size_t i = 0; i < _cellCount; i++)
	{
    Cell& cell = _cells[i];
    cell.data = nullptr;
	}
  _crtMemBlock = 0;
  _crtMemBlockOffset = 0;
}

void JLMBackBuffer::drawAALine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
   int16_t dx, dy, xdir;

   if (y1 > y2) 
   {
      std::swap(y1, y2);
      std::swap(x1, x2);
   }

  WritePixelFN wp;
  if (x1 < _clipX || y1 < _clipY || x2 > _clipX2 || y2 > _clipY2)
  {
    wp = &JLMBackBuffer::_writePixel;
  }
  else
  {
    wp = &JLMBackBuffer::_writePixelNC;
  }

   (this->*wp)(x1, y1, color, 0xFF);

   if ((dx = x2 - x1) >= 0) 
   {
      xdir = 1;
   } 
   else 
   {
      xdir = -1;
      dx = -dx; //make dx positive
   }
   
   if ((dy = y2 - y1) == 0) 
   {
      //Horizontal line
      while (dx-- != 0) 
      {
         x1 += xdir;
         (this->*wp)(x1, y1, color, 0xFF);
      }
      return;
   }
   if (dx == 0) 
   {
      //Vertical line
      do 
      {
         y1++;
         (this->*wp)(x1, y1, color, 0xFF);
      } while (--dy != 0);
      return;
   }
   if (dx == dy) 
   {
      //Diagonal line
      do {
         x1 += xdir;
         y1++;
         (this->*wp)(x1, y1, color, 0xFF);
      } while (--dy != 0);
      return;
   }
   uint16_t errorAcc = 0;
   if (dy > dx) 
   {
      uint16_t errorAdj = ((unsigned long) dx << 16) / (unsigned long) dy;
      while (--dy) 
      {
         uint16_t errorAccTemp = errorAcc;
         errorAcc += errorAdj;
         if (errorAcc <= errorAccTemp) 
         {
            x1 += xdir;
         }
         y1++;
         (this->*wp)(x1, y1, color, 0xFF - (errorAcc >> 8));
         (this->*wp)(x1 + xdir, y1, color, (errorAcc >> 8));
      }
      _writePixel(x2, y2, color, 0xFF);
      return;
   }
   uint16_t errorAdj = ((unsigned long) dy << 16) / (unsigned long) dx;
   while (--dx) 
   {
      uint16_t errorAccTemp = errorAcc;
      errorAcc += errorAdj;
      if (errorAcc <= errorAccTemp) 
      {
         y1++;
      }
      x1 += xdir;
      (this->*wp)(x1, y1, color, 0xFF - (errorAcc >> 8));
      (this->*wp)(x1, y1 + 1, color, (errorAcc >> 8));
   }
   (this->*wp)(x2, y2, color, 0xFF);
}
//adapted from http://members.chello.at/~easyfilter/bresenham.c
void JLMBackBuffer::drawAACircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{                    
  WritePixelFN wp;
  if (x0 - r - 1 < _clipX || y0 - r - 1 < _clipY || x0 + r + 1 > _clipX2 || y0 + r + 1 > _clipY2)
  {
    wp = &JLMBackBuffer::_writePixel;
  }
  else
  {
    wp = &JLMBackBuffer::_writePixelNC;
  }
  
   int16_t x = -r, y = 0;           // II. quadrant from bottom left to top right
   int16_t i, err = 2-2*r;          // error of 1 step
   r = 1-err;
   int32_t dr = 255*65536 / r;

   do 
   {
      i = 0xFF - (dr*std::abs(err-2*(x+y)-2)>>16);
      (this->*wp)(x0-x, y0+y, color, i);                             //   I. Quadrant
      (this->*wp)(x0-y, y0-x, color, i);                             //  II. Quadrant
      (this->*wp)(x0+x, y0-y, color, i);                             // III. Quadrant
      (this->*wp)(x0+y, y0+x, color, i);                             //  IV. Quadrant
      int16_t e2 = err; 
      int16_t x2 = x;
      if (err+y > 0) 
      {                                                             // x step
         i = 0xFF - (dr*(err-2*x-1)>>16);                           // outward pixel
         if (i > 0) 
         {
            (this->*wp)(x0-x, y0+y+1, color, i);
            (this->*wp)(x0-y-1, y0-x, color, i);
            (this->*wp)(x0+x, y0-y-1, color, i);
            (this->*wp)(x0+y+1, y0+x, color, i);
         }
         err += ++x*2+1;
      }
      if (e2+x2 <= 0) 
      {                                                             // y step
         i = 0xFF - (dr*(2*y+3-e2)>>16);                            // inward pixel
         if (i > 0) 
         {
            (this->*wp)(x0-x2-1, y0+y, color, i);
            (this->*wp)(x0-y, y0-x2-1, color, i);
            (this->*wp)(x0+x2+1, y0-y, color, i);
            (this->*wp)(x0+y, y0+x2+1, color, i);
         }
         err += ++y*2+1;
      }
   } while (x < 0);
}

//#define DEBUG_BLITTING

void JLMBackBuffer::blit(Adafruit_SPITFT& dst)
{
#ifdef DEBUG_BLITTING
  s_display.fillScreen(0);
#endif
  
	dst.startWrite();
 
  bool needsToWait = false;
	uint16_t cellIndex = 0;
	for (uint16_t j = 0; j < _cellCountY; j++)
	{
		for (uint16_t i = 0; i < _cellCountX; i++)
		{
			CellHash cellHash;
			Cell& cell = _cells[cellIndex];
      uint16_t* data = cell.data;
			if (!data)
			{
				data = _clearCell.data;
				cellHash = _clearCellHash;
			}
			else
			{
				cellHash = computeCellHash(cell);
			}

			if (cellHash != cell.prevHash)
			{
        if (needsToWait)
        {
          dst.dmaWait();
          needsToWait = false;
        }

#ifdef DEBUG_BLITTING
        static Cell highlightCell;
        if (highlightCell.data == nullptr)
        {
          //acquireCell(highlightCell);
          highlightCell.data = new uint16_t[_cellDataSize];
        }
        for (size_t x = 0; x < _cellDataSize; x++)
        {
          highlightCell.data[x] = blend(data[x], 0xFFFF, 127);
        }
        data = highlightCell.data;
        //memcpy(highlightCell.data, data, _cellDataSize * sizeof(uint16_t));
#endif

        int16_t x = i << _cellWBits;
        int16_t y = j << _cellHBits;
        if (x + _cellW <= _width && y + _cellH <= _height) //fast path, fully on screen
				{
				  dst.setAddrWindow(x, y, _cellW, _cellH);
          dst.writePixels(data, _cellDataSize, false);
          needsToWait = true;
				}
        else if (x + _cellW <= _width && y + _cellH > _height) //slower path, horizontally on screen but vertically clipping
        {
          int16_t h = _height - y;
          dst.setAddrWindow(x, y, _cellW, h);
          dst.writePixels(data, h << _cellWBits, false);
          needsToWait = true;
        }
        else //slowest path, horizontally and/or vertically clipping
        {
          int16_t w = std::min<int16_t>(_width - x, _cellW);
          int16_t h = std::min<int16_t>(_height - y, _cellH);
          dst.setAddrWindow(x, y, w, h);
          for (uint16_t jj = 0; jj < h; jj++)
          {
            dst.writePixels(data + (jj << _cellWBits), w);
          }
        }
				cell.prevHash = cellHash;
			}
      cell.data = nullptr;
			cellIndex++;
		}
	}
  if (needsToWait)
  {
    dst.dmaWait();
    needsToWait = false;
  } 
	dst.endWrite();

  _crtMemBlock = 0;
  _crtMemBlockOffset = 0;
}

void JLMBackBuffer::acquireCell(Cell& cell)
{
  //crt block full? allocate a new one
  if (_crtMemBlockOffset + _cellDataSize > _memBlockSize)
  {
    _crtMemBlock++;
    _crtMemBlockOffset = 0;
  }

  //need a new block?
  if (_crtMemBlock >= _memBlocks.size())
  {
    _memBlocks.push_back(MemBlock{new uint16_t[_memBlockSize]});
  }
  
  cell.data = _memBlocks[_crtMemBlock].data + _crtMemBlockOffset;
  memcpy(cell.data, _clearCell.data, _cellDataSize * sizeof(uint16_t));
  _crtMemBlockOffset += _cellDataSize;
}
void JLMBackBuffer::fillCell(Cell& cell, uint16_t color, uint8_t opacity)
{
	if (opacity <= 223)
	{
		uint8_t a32 = encodeAlpha(opacity);
		uint32_t c32 = encodeColor(color);
		uint16_t* ptr = cell.data;
		size_t sz = _cellDataSize;
		while (sz-- > 0)
		{
      uint16_t dst = *ptr;
      uint32_t bg = (dst | (dst << 16)) & 0b00000111111000001111100000011111;
      uint32_t result = (((c32 - bg) * a32) >> 5) + bg;
      result &= 0b00000111111000001111100000011111; // mask out fractional parts
      *ptr =  (int16_t)((result >> 16) | result); // contract result      
			ptr++;
		}
	}
	else if (color != 0)
	{
		//initialize the cell with the clear color
		uint16_t* ptr = cell.data;
    int16_t n = (_cellDataSize + 7) >> 3;
    switch (_cellDataSize & 7) {
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
	else
	{
		memset(cell.data, 0, _cellDataSize * sizeof(uint16_t));		
	}
}

static uint32_t murmur32(const void* _buffer, size_t size, uint32_t seed)
{
	const uint8_t* buffer = reinterpret_cast<const uint8_t*>(_buffer);

	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	const uint32_t m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	uint32_t h = seed ^ static_cast<uint32_t>(size);

	// Mix 4 bytes at a time into the hash

	while (size >= 4)
	{
		uint32_t k = *(uint32_t*)buffer;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		buffer += 4;
		size -= 4;
	}

	// Handle the last few bytes of the input array

	switch (size)
	{
	case 3: h ^= (buffer[2]) << 16;
	case 2: h ^= (buffer[1]) << 8;
	case 1: h ^= (buffer[0]);
		h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}

JLMBackBuffer::CellHash JLMBackBuffer::computeCellHash(Cell& cell) const
{
	CellHash hash = 0;
	hash = murmur32(cell.data, _cellDataSize * sizeof(uint16_t), 0);
	return hash;
}
