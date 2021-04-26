# jlm-back-buffer

This library implements a dirty-rectangle back buffer that can be used with the Adafruit SPITFT library (part of the Adafruit GFX library).<br/>
It allows you to have flicker-free, very fast update rates (more than 100 FPS is easily achievable on an ESP32 with an ILI9431 display) as long as you're not changing the whole screen every frame.<br/>

It works by only updating the data that actually changed from one frame to the next - therefore minimizing SPI transfers.<br/>

On top of this it adds alpha blending support so you can draw transparent graphics - either using the new blitRGBA8888 method or by calling setOpacity(x) and then using the regular Adafruit_GFX routines.<br/>
<br/>
There are 2 new primitives as well: drawAALine and drawAACircle that draw antialiased lines and circles. Their performance is around 50-70% of the non-antialiased versions (check the benchmark below for details).<br/>

# Details
The library splits the screen in cells of X by Y pixels, where X and Y are computed using the parameters passed in the constructor, like this:<br/>
`BackBuffer buffer(320, 240, 4, 4); // this will result in cells of (1 << 4) by (1 << 4) so 16x16 pixels`<br/>
`BackBuffer buffer(320, 240, 2, 2); // this will result in cells of (1 << 2) by (1 << 2) so 4x4 pixels`<br/>

The buffer then allocates cells form heap as they are needed, so if you only update half of the screen the buffer will allocate only half of the cells.<br/>
This behavior allows you to keep memory under control by limiting the area you're using.<br/>
Note that there is some extra overhead per cell - as it doesn't contain only the pixel data.<br/>

Screen clears using fillScreen are very fast and don't allocate cells.<br/>
There are specialized fillRect, drawHLine, drawVLine implementations that are way faster as they can take advantage of the cell structure<br/>

# Examples
`BackBuffer buffer(320, 240, 4, 4);`<br/>

For a screen of 320x240:
This will split the screen in 20x16 cells (320/16 by 240/16) so 300 cells.<br/>
It will allocate up-front around 2.5K of memory for housekeeping.<br/>
Then depending on how big is the area that you're changing compared to the previous frame, a vertain number of cells have to be allocated (just the first time they are used).<br/>
If you update only half of the screen (say top half), then only 150 cells will be allocated => so around 78K of memory (a full screen image at this resolution and bit depth is around 154K).<br/>
Note that these cells will stay allocated and will be reused regardless of where you draw on screen - so next frame if you render on the bottom half no extra allocation is made.<br/>

If you update the whole screen, then the memory used will be around 156K, so very little extra compared to a normal fullscreen image at 16 bits per pixel.<br/>

Another important thing is that the buffer doesn't need to preserve the old contents in memory to determine what changed from one frame to the next. It does this by hashing the cell contents so this reduces the memory requirements to half and increases performance as well.<br/>


# Benchmarks
The benchmark will test 7 primitives (fillCircle, fillRect, drawLine, drawAALine, drawCircle, drawAACircle, print) and print out the memory consumption and FPS for 5, 10, 50, 100, 500 and 1000 primitives per frame.<br/>
The 5 tests are:<br/>
* Adafruit GFX ILI9341 raw display with erase between frames. This test will output directly to the screen and then 'erase' its content by drawing with black. This results in a lot of flicker. Performance is very good when there are few primitives but drops severily as the number of draw commands increase - especially with text.<br/>
* Adafruit GFX ILI9341 raw display with clears between frames. This test will also output directly to the screen but between frames it will issue a clear (fillScreen(0)). There's a lot of flicker, but the FPS doesn't drop that much with many primitives as with the previous test. However, it's capped by the SPI transfer rate. I get 25 FPS with 5 primitives at 64MHz SPI speed.<br/>
* Adafruit GFX canvas16 - this is a modified canvas16 implementation as the original one doesn't fit in memory due to it's huge allocation (320x240x2 bytes -> 150K). The ESP32 doesn't have such a big block so it fails. My implementation splits the canvas in several pieces that fit in heap. There is no flicker and its performance is way better than any of the above methods but still capped due to the SPI speed, also to around 25 FPS. It degrades way less with the number of primitives and even wins some of the tests at the end.<br/>
* JLM solid - the back buffer from this library. This allocates memory incrementally and in small chunks so it always fits in heap (if there is enough heap of course). It's not capped by SPI speed so the FPS with few primitives (actually with few changed cells) is very high - you can also get 1000 FPS with just some text on screen. When the rendering gets very heavy it comes behind the canvas16 in some tests. However due to its optimal rect and h/v line routines, it's always several times faster in these tests.<br/>
* JLM alpha - same as the one above, but with alpha blending on (50% opacity)<br/>
<br/>
In bold are the best values in each category.<br/>
<br/>

|GFX|Memory|Count|Fill Circle|Fill Rect|Line|AA Line|Circle|AA Circle|Text|
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
|Adafruit raw & erase|**0**|5|58.22|66.82|78.51|N/A|56.00|N/A|21.52|
|Adafruit raw & clear|**0**|5|20.21|20.65|21.22|N/A|20.19|N/A|15.66|
|Adafruit canvas16|160K|5|24.90|24.84|25.43|N/A|25.45|N/A|24.87|
|JLM solid|**31K**|5|**235.43**|**252.83**|**329.11**|**301.13**|**238.10**|**221.93**|**158.23**|
|JLM alpha|**31K**|5|225.52|236.10|315.46|297.33|233.18|219.26|148.42|
|||||||||||
|Adafruit raw & erase|**0**|10|39.83|48.46|60.84|N/A|36.53|N/A|9.30|
|Adafruit raw & clear|**0**|10|18.69|19.50|20.47|N/A|18.38|N/A|10.93|
|Adafruit canvas16|160K|10|23.83|23.59|25.32|N/A|25.34|N/A|23.79|
|JLM solid|**55K**|10|**109.38**|**128.08**|**183.15**|**170.77**|**115.44**|**106.86**|**69.77**|
|JLM alpha|**55K**|10|110.11|114.03|174.61|168.72|116.14|107.71|64.74|
|||||||||||
|Adafruit raw & erase|**0**|50|12.30|16.68|25.17|N/A|10.35|N/A|1.47|
|Adafruit raw & clear|**0**|50|12.24|14.08|16.53|N/A|11.22|N/A|2.64|
|Adafruit canvas16|160K|50|18.14|17.23|24.51|N/A|24.63|N/A|16.99|
|JLM solid|**142K**|50|**35.31**|**41.22**|**51.30**|**48.47**|**34.47**|**32.39**|**18.76**|
|JLM alpha|**142K**|50|30.71|32.67|49.39|46.75|35.24|31.29|15.71|
|||||||||||
|Adafruit raw & erase|**0**|100|7.42|10.45|15.60|N/A|5.94|N/A|0.77|
|Adafruit raw & clear|**0**|100|9.20|11.25|13.72|N/A|8.00|N/A|1.46|
|Adafruit canvas16|160K|100|14.92|13.88|23.63|N/A|**23.90**|N/A|**13.02**|
|JLM solid|**152K**|100|**26.53**|**31.58**|**34.84**|**32.45**|**26.38**|**23.53**|12.31|
|JLM alpha|**152K**|100|22.07|22.74|32.60|30.98|25.13|22.67|10.57|
|||||||||||
|Adafruit raw & erase|**0**|200|4.13|6.02|8.99|N/A|3.21|N/A|0.39|
|Adafruit raw & clear|**0**|200|6.16|8.07|10.34|N/A|5.09|N/A|0.75|
|Adafruit canvas16|160K|200|11.01|10.01|22.08|N/A|**22.62**|N/A|**8.73**|
|JLM solid|**154K**|200|**20.05**|**23.13**|**23.72**|**21.75**|21.47|**18.12**|8.04|
|JLM alpha|**154K**|200|16.16|16.40|22.95|21.79|19.82|17.39|6.64|
|||||||||||
|Adafruit raw & erase|**0**|500|1.67|2.45|3.79|N/A|1.29|N/A|0.14|
|Adafruit raw & clear|**0**|500|2.93|4.07|5.76|N/A|2.34|N/A|0.28|
|Adafruit canvas16|160K|500|5.79|5.09|**18.35**|N/A|**19.27**|N/A|**4.00**|
|JLM solid|**154K**|500|**14.83**|**17.68**|16.96|**15.30**|16.82|**12.37**|3.71|
|JLM alpha|**154K**|500|10.43|10.49|16.27|15.26|14.85|12.24|2.92|
|||||||||||
|Adafruit raw & erase|**0**|1000|0.83|1.22|1.92|N/A|0.64|N/A|0.07|
|Adafruit raw & clear|**0**|1000|1.55|2.22|3.28|N/A|1.22|N/A|0.13|
|Adafruit canvas16|160K|1000|3.21|2.78|**14.33**|N/A|**15.40**|N/A|**2.09**|
|JLM solid|**154K**|1000|**10.66**|**12.94**|13.08|**11.28**|13.39|**8.49**|1.94|
|JLM alpha|**154K**|1000|6.72|6.75|12.24|11.13|11.01|8.30|1.50|

# API
## Clipping
  `void setClipRect(int16_t x, int16_t y, int16_t w, int16_t h);`<br/>
  This will clip all rendering to the specified rectangle.<br/>
  
## Alpha Blending (transparency)
  `void setOpacity(uint8_t opacity);`<br/>
  This will cause all rendering from now on to be transparent using the opacity from the argument.<br/>
  Note that there are only 32 levels of transparency, so values from 0 -> 7 are fully transparent, 8 -> 15 are equally transparent and so on.<br/>
  Pass 255 (0xFF) for opaque drawing.<br/>
  
## Blitting
  `void blit(Adafruit_SPITFT& dst);`<br/>
  Use this to blit the contents to the real hardware screen.<br/>
  Note that this will clear all the contents afterwards.<br/>
  

