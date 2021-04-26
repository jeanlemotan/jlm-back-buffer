#include "JLMBackBuffer.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_GFX.h"
#include "GFXCanvas16Split.h"
#include "bt.h"
#include "XPT2046.h"

#include "Fonts/FreeSansBold9pt7b.h"
#include "Fonts/FreeSansBold12pt7b.h"
#include "Fonts/FreeSansBold18pt7b.h"
#include "Fonts/FreeSansBold24pt7b.h"

#include "vec2.h"

Adafruit_ILI9341 s_display(5, 4);
XPT2046_Touchscreen s_touchscreen(27);

constexpr uint32_t k_runDuration = 3000;

constexpr size_t k_maxSpriteCount = 1000;
struct Sprite
{
  vec2f position;
  vec2f velocity;
  float r = 0;
  int16_t radius = 0;
  uint16_t color = 0;
};
static Sprite s_sprites[k_maxSpriteCount];

struct Plane
{
  float distance;
  vec2f normal;
};

Plane s_planes[] = 
{ 
  {0,    vec2f(1, 0)},  //left
  {-320, vec2f(-1, 0)}, //right
  {0,    vec2f(0, 1)},  //top
  {-240, vec2f(0, -1)}, //bottom
};

enum class Demo
{
  FillCircle,
  FillRect,
  Line,
  AALine,
  Circle,
  AACircle,
  Text,
  Count
};

const char* s_demoNames[] = 
{ 
  "Fill Circle", 
  "Fill Rect", 
  "Line", 
  "AA Line", 
  "Circle", 
  "AA Circle",
  "Text"
};

static bool s_collisions = false;

void drawSprite(Adafruit_GFX& canvas, JLMBackBuffer* buffer, const Sprite& spr, size_t index, Demo demo)
{
  if (demo == Demo::FillCircle)
  {
    canvas.fillCircle(spr.position.x, spr.position.y, spr.radius, spr.color);
  }
  else if (demo == Demo::FillRect)
  {
    canvas.fillRect(spr.position.x - spr.radius, spr.position.y - spr.radius, spr.radius*2, spr.radius*2, spr.color);
  }
  else if (demo == Demo::Line || demo == Demo::AALine)
  {
    float c = cos(spr.r);
    float s = sin(spr.r);
    vec2f dir{c * spr.radius, s * spr.radius};
    vec2f l1 = spr.position - dir;
    vec2f l2 = spr.position + dir;
    if (demo == Demo::Line)
    {
      canvas.drawLine(l1.x, l1.y, l2.x, l2.y, spr.color);
    }
    else
    {
      if (buffer)
      {
        buffer->drawAALine(l1.x, l1.y, l2.x, l2.y, spr.color);
      }
      else
      {
        canvas.drawLine(l1.x, l1.y, l2.x, l2.y, spr.color);
      }
    }
  }
  else if (demo == Demo::Circle)
  {
    canvas.drawCircle(spr.position.x, spr.position.y, spr.radius, spr.color);
  }
  else if (demo == Demo::AACircle)
  {
    if (buffer)
    {
      buffer->drawAACircle(spr.position.x, spr.position.y, spr.radius, spr.color);
    }
    else
    {
      canvas.drawCircle(spr.position.x, spr.position.y, spr.radius, spr.color);
    }
  }
  else if (demo == Demo::Text)
  {
    if (spr.radius >= 24) canvas.setFont(&FreeSansBold24pt7b);
    else if (spr.radius >= 18) canvas.setFont(&FreeSansBold18pt7b);
    else if (spr.radius >= 12) canvas.setFont(&FreeSansBold12pt7b);
    else canvas.setFont(&FreeSansBold9pt7b);

    canvas.setTextColor(spr.color);
    canvas.setCursor(spr.position.x, spr.position.y);
    canvas.print("Text");
    canvas.print(index);

    canvas.setFont(nullptr);
  }
}

void drawTitle(Adafruit_GFX& canvas, JLMBackBuffer* buffer, size_t count, const char* title, float fps, uint16_t color, Demo demo)
{
  //canvas.setFont(m_font);
  canvas.setTextColor(color);
  canvas.setTextSize(1);
  canvas.setCursor(10, canvas.height() - 10);
  canvas.print(title);
  canvas.print(", ");
  canvas.print(count);
  canvas.print(" ");
  canvas.print(s_demoNames[(size_t)demo]);
  canvas.print("s, ");
  canvas.print(fps);
  canvas.print(" FPS");
}

bool collide(Sprite& a, Sprite& b)
{
  // separation vector
  vec2f d(b.position - a.position);
  
  // distance between circle centres, squared
  float distance_squared = length_sq(d);
  
  // combined radius squared
  float radius = b.radius + a.radius;
  float radius_squared = radius * radius;
  
  // circles too far apart
  if (distance_squared > radius_squared) 
  {
    return false;
  }
  
  // distance between circle centres
  float distance = (float)std::sqrt(distance_squared);
  
  // normal of collision
  vec2f ncoll = d / distance;
  
  // penetration distance
  float dcoll = (radius - distance);
  
  // inverse masses (0 means, infinite mass, object is static).
  float amass = a.radius*a.radius;
  float bmass = b.radius*b.radius;
  float ima = 1.0f / amass;
  float imb = 1.0f / bmass;
  
  // separation vector
  vec2f separation_vector = ncoll * (dcoll / (ima + imb));
  
  // separate the circles
  a.position -= separation_vector * ima;
  b.position += separation_vector * imb;
  
  // combines velocity
  vec2f vcoll = (b.velocity - a.velocity);
  
  // impact speed 
  float vn = dot(vcoll, ncoll);
  
  // obejcts are moving away. dont reflect velocity
  if (vn > 0.0f) 
  {
    return true; // we did collide
  }
  
  // coefficient of restitution in range [0, 1].
  const float cor = 0.95f; // air hockey -> high cor
  
  // collision impulse
  float j = -(1.0f + cor) * (vn) / (ima + imb);
  
  // collision impusle vector
  vec2f impulse = j * ncoll;
  
  // change momentum of the circles
  a.velocity -= impulse * ima;
  b.velocity += impulse * imb;
  
  // collision reported
  return true;
}

void doDemo(Adafruit_GFX& canvas, JLMBackBuffer* buffer, bool erase, size_t count, const char* title, float fps, uint8_t opacity, Demo demo)
{
  if (buffer)
  {
    buffer->setOpacity(0xFF);
  }

  if (erase)
  {
    for (size_t i = 0; i < count; i++)
    {
      Sprite& spr = s_sprites[i];
      uint16_t oldColor = spr.color;
      spr.color = 0;
      drawSprite(canvas, buffer, spr, i, demo);
      spr.color = oldColor;
    }

    static float oldFps = fps;
    drawTitle(canvas, buffer, count, title, oldFps, 0, demo);
    oldFps = fps;
  }
    
  drawTitle(canvas, buffer, count, title, fps, 0xFFFF, demo);

  if (buffer)
  {
    buffer->setOpacity(opacity);
  }

  for (size_t i = 0; i < count; i++)
  {
    Sprite& spr = s_sprites[i];

    if (s_collisions)
    {
      for (size_t j = i + 1; j < count; j++)
      {
        Sprite& spr2 = s_sprites[j];
        collide(spr, spr2);
      }
    }
    for (const Plane& p: s_planes)
    {
      float d = dot(spr.position, p.normal);
      if (d - spr.radius < p.distance)
      {
        vec2f r = spr.velocity - p.normal * 2 * dot(spr.velocity, p.normal);
        spr.velocity = r;
      }
    }
  }  
  
  for (size_t i = 0; i < count; i++)
  {
    Sprite& spr = s_sprites[i];
    spr.position += spr.velocity;
    spr.r += spr.velocity.x * 0.1f;

    drawSprite(canvas, buffer, spr, i, demo);
  }

  drawTitle(canvas, buffer, count, title, fps, 0xFFFF, demo);
}

enum class Mode
{
  Cycle,
  AdaErase,
  AdaClear,
  AdaCanvas,
  JLMSolid,
  JLMAlpha,
  Count
};

Mode s_mode = Mode::Cycle;

const char* s_modeNames[] = 
{
  "Cycle",
  "Ada Erase",
  "Ada Clear",
  "Ada BB",
  "JLM BB S",
  "JLM BB A",
};

bool s_touched = false;

bool drawUI(Adafruit_GFX& canvas)
{
  bool done = false;
  bool touched = s_touchscreen.touched();
  Touchscreen::Point point = s_touchscreen.getPoint();
  point.x = point.x / 4096.f * 320;
  point.y = point.y / 4096.f * 240;

  int16_t w = 60;
  int16_t h = 24;
  int16_t y = 0;
  for (size_t i = 0; i < (size_t)Mode::Count + 1; i++)
  {
    bool pressed = touched && touched != s_touched && point.x < w && point.y >= y && point.y <= y + h;
    if (pressed)
    {
      if (i < (size_t)Mode::Count)
      {
        s_mode = (Mode)(i);
        done = true;
        printf("\nSwitching mode to %d\n", (int)i);
      }
      else
      {
        s_collisions = !s_collisions;
      }
    }
    canvas.fillRect(0, y, w, h, s_mode == Mode(i) ? 0xFFFF : 0x0);
    canvas.drawRect(0, y, w, h, 0xFFFF);
    canvas.setTextColor(s_mode == Mode(i) ? 0x0 : 0xFFFF);
    canvas.setCursor(2, y + h / 2);
    if (i < (size_t)Mode::Count)
    {
      canvas.print(s_modeNames[i]);
    }
    else
    {
      canvas.printf("Coll: %s", s_collisions ? "Yes" : "No");
    }
    y += h + 2;
  }

  s_touched = touched;
  
  return done;
}

bool testAdaRawErase(size_t count)
{
  float results[(size_t)Demo::Count];
  int memUsed = 0;
  
  int bmem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  {
    for (size_t i = 0; i < (size_t)Demo::Count; i++)
    {
      s_display.fillScreen(0);
      uint32_t start = millis();
      int frames = 0;
      while (millis() - start < k_runDuration)
      {
        float fps = frames / ((millis() - start + 1) / 1000.f);
        doDemo(s_display, nullptr, true, count, "Ada Erase", fps, 0xFF, (Demo)i);
        if (drawUI(s_display))
        {
          return true;
        }
        frames++;
      }
      uint32_t duration = millis() - start;
      results[i] = frames / (duration / 1000.f);
      int emem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      memUsed = std::max(bmem - emem, memUsed);
    }  
  }

  printf("Adafruit raw & erase,%d,%d,", memUsed, (int)count);
  for (size_t i = 0; i < (size_t)Demo::Count; i++)
  {
    printf("%f,", results[i]);
  }
  printf("\n");
  return false;
}

bool testAdaRawClear(size_t count)
{
  float results[(size_t)Demo::Count];
  int memUsed = 0;

  int bmem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  {  
    for (size_t i = 0; i < (size_t)Demo::Count; i++)
    {
      s_display.fillScreen(0);
      uint32_t start = millis();
      int frames = 0;
      while (millis() - start < k_runDuration)
      {
        s_display.fillScreen(0);
        float fps = frames / ((millis() - start + 1) / 1000.f);
        doDemo(s_display, nullptr, false, count, "Ada Clear", fps, 0xFF, (Demo)i);
        if (drawUI(s_display))
        {
          return true;
        }
        frames++;
      }
      uint32_t duration = millis() - start;
      results[i] = frames / (duration / 1000.f);
      int emem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      memUsed = std::max(bmem - emem, memUsed);
    }
  }

  printf("Adafruit raw & clear,%d,%d,", memUsed, (int)count);
  for (size_t i = 0; i < (size_t)Demo::Count; i++)
  {
    printf("%f,", results[i]);
  }
  printf("\n");
  return false;
}

bool testAdaCanvas(size_t count)
{
  float results[(size_t)Demo::Count];
  int memUsed = 0;

  int bmem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  {  
    GFXCanvas16Split buffer(s_display.width(), s_display.height());
    buffer.fillScreen(0);
    
    for (size_t i = 0; i < (size_t)Demo::Count; i++)
    {
      uint32_t start = millis();
      int frames = 0;
      while (millis() - start < k_runDuration)
      {
        buffer.fillScreen(0);
        float fps = frames / ((millis() - start + 1) / 1000.f);
        doDemo(buffer, nullptr, false, count, "Ada Canvas", fps, 0xFF, (Demo)i);
        bool done = drawUI(buffer);
        buffer.blit(s_display);
        if (done)
        {
          return true;
        }
        frames++;
      }
      uint32_t duration = millis() - start;
      results[i] = frames / (duration / 1000.f);
      int emem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      memUsed = std::max(bmem - emem, memUsed);
    }  
  }
  printf("Adafruit canvas16,%d,%d,", memUsed, (int)count);
  for (size_t i = 0; i < (size_t)Demo::Count; i++)
  {
    printf("%f,", results[i]);
  }
  printf("\n");
  return false;
}

bool testJLMSolid(size_t count)
{
  float results[(size_t)Demo::Count];
  int memUsed = 0;

  int bmem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  {  
    JLMBackBuffer buffer(s_display.width(), s_display.height(), 4, 4);
    buffer.fillScreen(0);
    buffer.blit(s_display);

    for (size_t i = 0; i < (size_t)Demo::Count; i++)
    {
      uint32_t start = millis();
      int frames = 0;
      while (millis() - start < k_runDuration)
      {
        float fps = frames / ((millis() - start + 1) / 1000.f);
        doDemo(buffer, &buffer, false, count, "JLM solid", fps, 0xFF, (Demo)i);
        bool done = drawUI(buffer);
        buffer.blit(s_display);
        if (done)
        {
          return true;
        }
        frames++;
      }
      uint32_t duration = millis() - start;
      results[i] = frames / (duration / 1000.f);
      int emem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      memUsed = std::max(bmem - emem, memUsed);
    }
  }
  printf("JLM solid,%d,%d,", memUsed, (int)count);
  for (size_t i = 0; i < (size_t)Demo::Count; i++)
  {
    printf("%f,", results[i]);
  }
  printf("\n");
  return false;
}

bool testJLMAlpha(size_t count)
{
  float results[(size_t)Demo::Count];
  int memUsed = 0;

  int bmem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  {
    JLMBackBuffer buffer(s_display.width(), s_display.height(), 4, 4);
    buffer.fillScreen(0);
    buffer.blit(s_display);
    
    for (size_t i = 0; i < (size_t)Demo::Count; i++)
    {
      uint32_t start = millis();
      int frames = 0;
      while (millis() - start < k_runDuration)
      {
        float fps = frames / ((millis() - start + 1) / 1000.f);
        doDemo(buffer, &buffer, false, count, "JLM alpha", fps, 127, (Demo)i);
        bool done = drawUI(buffer);
        buffer.blit(s_display);
        if (done)
        {
          return true;
        }
        frames++;
      }
      uint32_t duration = millis() - start;
      results[i] = frames / (duration / 1000.f);
      int emem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      memUsed = std::max(bmem - emem, memUsed);
    }  
  }

  printf("JLM alpha,%d,%d,", memUsed, (int)count);
  for (size_t i = 0; i < (size_t)Demo::Count; i++)
  {
    printf("%f,", results[i]);
  }
  printf("\n");
  return false;
}

void setup() 
{
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
  
  Serial.begin(115200);
  s_display.begin(64000000);
  s_display.setRotation(1);
  s_display.fillScreen(0);

  s_touchscreen.begin();
  s_touchscreen.setRotation(1);
  
  srand(millis());

  for (size_t i = 0; i < k_maxSpriteCount; i++)
  {
    Sprite& spr = s_sprites[i];
    //initialize spr
    spr.radius = 1 + rand() % 8;
    spr.color = (rand() % (0xFFFF / 2)) + 0xFFFF / 2;
    spr.position.x = rand() % (s_display.width() - spr.radius*2) + spr.radius;
    spr.position.y = rand() % (s_display.height() - spr.radius*2) + spr.radius;
    spr.velocity.x = (rand() % 1024) / 512.f - 1.f;
    spr.velocity.y = (rand() % 1024) / 512.f - 1.f;
  }

  s_planes[1].distance = -s_display.width();
  s_planes[3].distance = -s_display.height();
}

void loop()
{
  if (s_mode == Mode::AdaErase)
  {
    while (true)
    {
      s_display.fillScreen(0);
      if (testAdaRawErase(100))
      {
        break;
      }
    }
  }
  else if (s_mode == Mode::AdaClear)
  {
    while (true)
    {
      s_display.fillScreen(0);
      if (testAdaRawClear(100))
      {
        break;
      }
    }
  }
  else if (s_mode == Mode::AdaCanvas)
  {
    while (true)
    {
      s_display.fillScreen(0);
      if (testAdaCanvas(100))
      {
        break;
      }
    }
  }
  else if (s_mode == Mode::JLMSolid)
  {
    while (true)
    {
      s_display.fillScreen(0);
      if (testJLMSolid(100))
      {
        break;
      }
    }
  }
  else if (s_mode == Mode::JLMAlpha)
  {
    while (true)
    {
      s_display.fillScreen(0);
      if (testJLMAlpha(100))
      {
        break;
      }
    }
  }
  else if (s_mode == Mode::Cycle)
  {
    printf("GFX,Memory,Count,");
    for (size_t i = 0; i < (size_t)Demo::Count; i++)
    {
      printf("%s,", s_demoNames[i]);
    }
    printf("\n");
    
    size_t counts[] = { 10, 50, 100, 200, 500 };
    for (size_t ci = 0; ci < 5; ci++)
    {
      size_t count = counts[ci];
  
      s_display.fillScreen(0);
      if (testAdaRawErase(count))
      {
        break;
      }
  
      s_display.fillScreen(0);
      if (testAdaRawClear(count))
      {
        break;
      }
  
      s_display.fillScreen(0);
      if (testAdaCanvas(count))
      {
        break;
      }
  
      s_display.fillScreen(0);
      if (testJLMSolid(count))
      {
        break;
      }
  
      s_display.fillScreen(0);
      if (testJLMAlpha(count))
      {
        break;
      }
    }
  }
}
