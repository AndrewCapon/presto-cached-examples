#include "libraries/pico_graphics/pico_graphics.hpp"
#include "libraries/pico_vector/pico_vector.hpp"
#include "drivers/st7701/st7701Cached.hpp"

#include "ff.h"

#include "pico/multicore.h"
#include "pico/sync.h"

#include "hardware/dma.h"

#include <math.h>
#include <vector>

#include "PicoPlusPsram.h"
#include "Elapsed.h"

void *pCore1Psram = nullptr;

using namespace pimoroni;

FATFS fs;
FIL fil;
FIL audio_file;
FRESULT fr;

#define FRAME_WIDTH 480
#define FRAME_HEIGHT 480
static const uint BACKLIGHT = 45;
static const uint LCD_CLK = 26;
static const uint LCD_CS = 28;
static const uint LCD_DAT = 27;
static const uint LCD_DC = -1;
static const uint LCD_D0 = 1;

// we write to the back, the display scans out from the front
#define USE_PSRAM_FRONT 0
#define USE_PSRAM_BACK 1
#define USE_PSRAM_DOUBLE_BACK 1
#define FULL_CLEAR 3
#define USE_UNCACHED 1
#define BLOCK_COUNT 40
#define USE_CORE_1 0

#define CACHE_LINES 3
#define FRONT_BUFFER_SIZE FRAME_WIDTH*CACHE_LINES
#if USE_PSRAM_BACK
uint16_t *back_buffer;
#if USE_PSRAM_DOUBLE_BACK
uint16_t *back_buffer2;
#endif
#else
uint16_t back_buffer[FRAME_WIDTH * FRAME_HEIGHT];
#endif

#if USE_PSRAM_FRONT
uint16_t *front_buffer;
#else
uint16_t front_buffer[FRONT_BUFFER_SIZE];
#endif

ST7701Cached *presto;
PicoGraphics_PenRGB565 *display;
PicoVector *vector;

struct pt
{
  float x;
  float y;
  float xVeryOld;
  float yVeryOld;
  float xOld;
  float yOld;
  float dx;
  float dy;
  uint16_t use_pen;
};

std::vector<pt> pixels;

#define PIX_WH 16

#if USE_CORE_1
void Core1(void)
{
  volatile uint8_t *p = (volatile uint8_t *)pCore1Psram;
  while(true)
  {
    for(uint i = 0; i < (1024*32); i++)
      p[i] =p[i+1024*32];
  }
}
#endif

int main()
{
  volatile uint32_t uClock = 266000;
  set_sys_clock_khz(uClock, true);
  stdio_init_all();

  sleep_ms(1000);

  PicoPlusPsram &ps = PicoPlusPsram::getInstance();
  size_t uMemorySize = ps.GetMemorySize();

  gpio_init(LCD_CS);
  gpio_put(LCD_CS, 1);
  gpio_set_dir(LCD_CS, 1);

  // sleep_ms(5000);
  printf("PSRAM = %x\n", uMemorySize);

#if USE_CORE_1
  pCore1Psram = malloc(1024*64);

  multicore_launch_core1(Core1);
#endif

#if USE_PSRAM_BACK
#if USE_UNCACHED
  back_buffer = (uint16_t *)ps.Malloc(FRAME_WIDTH * FRAME_HEIGHT * 2) + 0x02000000;
#else
  back_buffer = (uint16_t *)ps.Malloc(FRAME_WIDTH * FRAME_HEIGHT * 2);
#endif

  //uint16_t *pUncachedBackBuffer = back_buffer + 0x06000000;
  //uint16_t *pUncachedBackBuffer = back_buffer + 0x02000000;

#if USE_PSRAM_DOUBLE_BACK
#if USE_UNCACHED
  back_buffer2 = (uint16_t *)ps.Malloc(FRAME_WIDTH * FRAME_HEIGHT * 2) + 0x02000000;
#else
  back_buffer2 = (uint16_t *)ps.Malloc(FRAME_WIDTH * FRAME_HEIGHT * 2);
#endif
#endif
#endif

#if USE_PSRAM_FRONT
  front_buffer = (uint16_t *)ps.Malloc(FRONT_BUFFER_SIZE * 2);
#endif

  
//  presto = new ST7701Cached(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0, SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT}, CACHE_LINES, front_buffer, (uint16_t *)back_buffer);
  presto = new ST7701Cached(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0, SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT}, (uint16_t *)back_buffer);
  display = new PicoGraphics_PenRGB565(FRAME_WIDTH, FRAME_HEIGHT, (uint16_t *)back_buffer);

  vector = new PicoVector(display);


  printf("Init: ...");
  presto->init();
  printf("Init: OK\n");

  memset(back_buffer, 0, FRAME_WIDTH * FRAME_HEIGHT *2);
#if USE_PSRAM_DOUBLE_BACK
  memset(back_buffer2, 0, FRAME_WIDTH * FRAME_HEIGHT *2);
#endif

  pixels.clear();

  for (int i = 0; i < BLOCK_COUNT; i++)
  {
    pt pixel;
    pixel.x = rand() % (display->bounds.w - PIX_WH);
    pixel.y = rand() % (display->bounds.h - PIX_WH);
    pixel.xOld = pixel.x;
    pixel.yOld = pixel.y;
    pixel.xVeryOld = pixel.x;
    pixel.yVeryOld = pixel.y;
    
    pixel.dx = float(rand() % 255) / 128.0f;
    pixel.dy = float(rand() % 255) / 128.0f;
    pixel.use_pen = display->create_pen(rand() % 255, rand() % 255, rand() % 255);
    pixels.push_back(pixel);
  }

  // presto starts back
  // display starts front
  bool bDisplayFront = true;
  display->set_pen(0);
  display->clear();


  while (true)
  {
    uint vsyncTime = 0;
    uint updateTime = 0;

    Elapsed timer;

#if FULL_CLEAR == 1
    // display->set_pen(0);
    // display->clear();
#if USE_PSRAM_DOUBLE_BACK    
    if (bDisplayFront)
      memset(back_buffer2, 0, FRAME_WIDTH * FRAME_HEIGHT *2);
    else
      memset(back_buffer, 0, FRAME_WIDTH * FRAME_HEIGHT *2);
#else
    memset(back_buffer, 0, FRAME_WIDTH * FRAME_HEIGHT *2);
#endif
#elif FULL_CLEAR == 2
    display->set_pen(0);
    for(auto &pixel : pixels) {
      display->rectangle({pixel.x, pixel.y, PIX_WH, PIX_WH});
    }
#endif // FULL CLEAR
    uint clearTime = timer.elapsedUs();

    for (auto &pixel : pixels)
    {
      pixel.xVeryOld = pixel.xOld;
      pixel.yVeryOld = pixel.yOld;
      pixel.xOld = pixel.x;
      pixel.yOld = pixel.y;

      pixel.x += pixel.dx;
      pixel.y += pixel.dy;
      if (pixel.x < 0)
        pixel.dx *= -1;
      if (pixel.x >= display->bounds.w - PIX_WH)
        pixel.dx *= -1;
      if (pixel.y < 0)
        pixel.dy *= -1;
      if (pixel.y >= display->bounds.h - PIX_WH)
        pixel.dy *= -1;
    }

    uint calcTime = timer.elapsedUs();
#if USE_PSRAM_DOUBLE_BACK 
#if FULL_CLEAR == 3     
    display->set_pen(0);
    for(auto &pixel : pixels) {
      display->rectangle({(int32_t)pixel.xVeryOld, (int32_t)pixel.yVeryOld, PIX_WH, PIX_WH});
    }
    clearTime = timer.elapsedUs();
#endif
#define TEST_DISPLAY 0
#if TEST_DISPLAY
    display->set_pen(display->create_pen(255,0,0));
    display->rectangle({0,0,FRAME_WIDTH,FRAME_HEIGHT});

    display->set_pen(display->create_pen(0,0,0));
    display->rectangle({1,1,FRAME_WIDTH-2,FRAME_HEIGHT-2});

    int y;
    display->set_pen(display->create_pen(0,0,255));
    y = 0;
    display->line({0,y}, {(FRAME_WIDTH-1)/2,y});

    display->set_pen(display->create_pen(0,255,0));
    y = FRAME_WIDTH-1;
    display->line({0,y}, {(FRAME_WIDTH-1)/2,y});
#endif //TEST_DISPLAY

    for(auto &pixel : pixels) {
      display->set_pen(pixel.use_pen);
      display->rectangle({pixel.x, pixel.y, PIX_WH, PIX_WH});
    }

#else
    for(auto &pixel : pixels) {
#if FULL_CLEAR == 3         
      display->set_pen(0);
      display->rectangle({pixel.xOld, pixel.yOld, PIX_WH, PIX_WH});
#endif
      display->set_pen(pixel.use_pen);
      display->rectangle({pixel.x, pixel.y, PIX_WH, PIX_WH});
    }
#endif

    // display->set_pen(rand());
    // display->rectangle({20, 20, 200, 200});
    // display->set_pen(0x00ff);
    // display->rectangle({100, 100, 200, 200});
    // display->set_pen(0xff00);
    // display->rectangle({000, 000, 100, 100});
    // display->set_pen(0);
    // vector->text("Hello World, how are you today?", 240, 240, &t);
    // display->text("Hello", {30, 30}, 240);
    uint drawTime = timer.elapsedUs();


#define VSYNC 0
#if VSYNC
    presto->wait_for_vsync();
    bDisplayFront = !bDisplayFront;
    if (bDisplayFront)
    {
      display->set_framebuffer(front_buffer);
      presto->set_framebuffer(back_buffer);
    }
    else
    {
      display->set_framebuffer(back_buffer);
      presto->set_framebuffer(front_buffer);
    }
    vsyncTime = timer.elapsed();
#else
#if USE_PSRAM_DOUBLE_BACK
    // presto->wait_for_vsync();
    // vsyncTime = timer.elapsed();

    bDisplayFront = !bDisplayFront;
    if (bDisplayFront)
    {
      display->set_framebuffer(back_buffer2);
      presto->set_backbuffer(back_buffer);
    }
    else
    {
      display->set_framebuffer(back_buffer);
      presto->set_backbuffer(back_buffer2);
    }
#endif

    presto->wait_for_vsync();
    vsyncTime = timer.elapsedUs();
    //presto->update(display);
    updateTime = timer.elapsedUs();
#endif
    printf("C=%u.%.2u ", calcTime / 1000, (calcTime - ((calcTime / 1000) * 1000)) / 10);
    printf("X=%u.%.2u ", clearTime / 1000, (clearTime - ((clearTime / 1000) * 1000)) / 10);
    printf("D=%u.%.2u ", drawTime / 1000, (drawTime - ((drawTime / 1000) * 1000)) / 10);
    printf("V=%u.%.2u ", vsyncTime / 1000, (vsyncTime - ((vsyncTime / 1000) * 1000)) / 10);
    printf("U=%u.%.2u ", updateTime / 1000, (updateTime - ((updateTime / 1000) * 1000)) / 10);
    uint totalTime = calcTime + clearTime + drawTime + vsyncTime + updateTime;
    printf("T=%u.%.2u ", totalTime / 1000, (totalTime - ((totalTime / 1000) * 1000)) / 10);
    uint fps = (float)1000000000 / (float)totalTime;
    printf("F=%u.%.2u\n", fps / 1000, (fps - ((fps / 1000) * 1000)) / 10);
  }
  
}

// #include "libraries/pico_graphics/pico_graphics.hpp"
// #include "libraries/pico_vector/pico_vector.hpp"
// #include "drivers/st7701/st7701.hpp"

// #include "ff.h"

// #include "pico/multicore.h"
// #include "pico/sync.h"

// #include "hardware/dma.h"

// using namespace pimoroni;

// FATFS fs;
// FIL fil;
// FIL audio_file;
// FRESULT fr;

// #define BUFFERS 1

// #if BUFFERS == 2
// #define FRAME_WIDTH 480
// #define FRAME_HEIGHT 480
// #else
// #define FRAME_WIDTH 240
// #define FRAME_HEIGHT 240
// #endif
// static const uint BACKLIGHT = 45;
// static const uint LCD_CLK = 26;
// static const uint LCD_CS = 28;
// static const uint LCD_DAT = 27;
// static const uint LCD_DC = -1;
// static const uint LCD_D0 = 1;

// #if BUFFERS == 2
// uint16_t back_buffer[FRAME_WIDTH * FRAME_HEIGHT];
// #endif
// uint16_t front_buffer[FRAME_WIDTH * FRAME_HEIGHT];

// ST7701 *presto;
// PicoGraphics_PenRGB565 *display;
// PicoVector *vector;

// int main()
// {
//   bool bGotFont = false;
//   bool bGotSdCard = false;

//   set_sys_clock_khz(240000, true);
//   stdio_init_all();

//   gpio_init(LCD_CS);
//   gpio_put(LCD_CS, 1);
//   gpio_set_dir(LCD_CS, 1);

//   //sleep_ms(5000);
//   printf("Hello\n");

//   fr = f_mount(&fs, "", 1);
//   bGotSdCard = (fr == FR_OK);

// #if BUFFERS == 2
//   presto = new ST7701(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0, SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT}, back_buffer);
// #else
//   presto = new ST7701(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0, SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT}, front_buffer);
// #endif

//   display = new PicoGraphics_PenRGB565(FRAME_WIDTH, FRAME_HEIGHT, front_buffer);
//   vector = new PicoVector(display);

//   printf("Init: ...");
//   presto->init();
//   printf("Init: OK\n");

//   if (bGotSdCard)
//   {
//     printf("Font: ...");
//     if (vector->set_font("/marker-high.af", 30))
//     {
//       printf("Font: OK\n");
//       bGotFont = true;
//     }
//     else
//     {
//       printf("Font: Fail!\n");
//     }
//   }

//   //sleep_ms(1000);

//   pp_mat3_t t = pp_mat3_identity();
//   pp_mat3_translate(&t, 50, 50);

//   while (true)
//   {
//     display->set_pen(0);
//     display->clear();
//     display->set_pen(rand());
//     display->rectangle({20, 20, 200, 200});
//     if (bGotFont)
//     {
//       display->set_pen(0);
//       vector->text("Hello World, how are you today?", 240, 240, &t);
//       display->text("Hello", {30, 30}, 240);
//     }
// //#if BUFFERS == 2
//     presto->update(display);
// //#endif
// //    while(true);
//   }
// }