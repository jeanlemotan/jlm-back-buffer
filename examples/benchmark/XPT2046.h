/* Touchscreen library for XPT2046 Touch Controller Chip
 * Copyright (c) 2015, Paul Stoffregen, paul@pjrc.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "Arduino.h"
#include <SPI.h>
#include "Touchscreen.h"

#if ARDUINO < 10600
#error "Arduino 1.6.0 or later (SPI library) is required"
#endif

class XPT2046_Touchscreen : public Touchscreen 
{
public:
	constexpr XPT2046_Touchscreen(uint8_t cspin, uint8_t tirq=255)
		: csPin(cspin), tirqPin(tirq) { }
	bool begin();
	Point getPoint() override;
	bool tirqTouched();
	bool touched() override;
	void readData(uint16_t *x, uint16_t *y, uint8_t *z);
	bool bufferEmpty();
	uint8_t bufferSize() { return 1; }
	void setRotation(uint8_t n) override { rotation = n % 4; }
// protected:
	volatile bool isrWake=true;

private:
	void update();
	uint8_t csPin, tirqPin, rotation=1;
	int16_t xraw=0, yraw=0, zraw=0;
	uint32_t msraw=0x80000000;
};
