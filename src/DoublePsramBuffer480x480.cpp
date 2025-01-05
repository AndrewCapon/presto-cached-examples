// ******************************************************************************
// This example shows two back buffers stored in PSRAM with
// the Presto running at 480x480 and using the ST7701Cached class.
//
// ST7701Cached uses a small cache of pico ram in order to send the
// data to the display, this saves pico memory as a whole framebuffers
// worth of data is not needed. Instead it uses 2880 bytes of memory
// for its cached data.
//
// The original ST7701 class works by only updateing the rows of the screen 
// that are not being currently sent out to the display when you call Update(), 
// this requires a whole frame buffer of data in the pico ram, and cleverly 
// enables you to use a single back buffer and get glitch free graphics.
//
// As ST7701Cached is only using a small amount of pico memory it cannot
// work in the same way, you don't even need to call Update() as it is constantly
// sending the current back buffer out to the display.
//
// To get glitch free graphics you need to use two back buffers and swap between them
// This means you need to keep these back buffers in sync, the psram is pretty
// slow on the presto, so clearing or copying a buffer every frame or can be quite costly.
//
// This example bounces some boxes around, there are different clear stratagies that
// you can set using the define CLEAR_TYPE to see the speed differences.
//
// Note: Timings and fps data are logged to the USB UART, if you attach a 
//       serial monitor to see them then you may get some slight glitching
//       with the display.
// ******************************************************************************

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "drivers/st7701/st7701Cached.hpp"

#include "PicoPlusPsram.h"
#include "Elapsed.h"
#include "FT6236.h"

using namespace pimoroni;

#define FRAME_WIDTH 480
#define FRAME_HEIGHT 480

static const uint BACKLIGHT = 45;
static const uint LCD_CLK = 26;
static const uint LCD_CS = 28;
static const uint LCD_DAT = 27;
static const uint LCD_DC = -1;
static const uint LCD_D0 = 1;

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
#define BLOCK_COUNT 100

// CLEAR_TYPE 0 = no clear, 1 = partial clear, 2 = memset clear, 3 = picographics clear
#define CLEAR_TYPE 1

FT6236 touchDisplay;

uint16_t                *back_buffers[2]; // Two back buffers to use
ST7701Cached            *presto;          // Sends data to the display
PicoGraphics_PenRGB565  *graphics;        // We draw with this

int main()
{
  // run as 266mhz, twice the speed of the Psram
  set_sys_clock_khz(266000, true);
  stdio_init_all();

  // Display available Psram
  PicoPlusPsram &ps = PicoPlusPsram::getInstance();
  size_t uMemorySize = ps.GetMemorySize();
  printf("PSRAM = %x\n", uMemorySize);

  // Set up the chip select
  gpio_init(LCD_CS);
  gpio_put(LCD_CS, 1);
  gpio_set_dir(LCD_CS, 1);

  // allocate 480x480 back buffers in psram, use uncached address
  back_buffers[0] = (uint16_t *)ps.Malloc(FRAME_WIDTH * FRAME_HEIGHT * 2) + 0x02000000;
  back_buffers[1] = (uint16_t *)ps.Malloc(FRAME_WIDTH * FRAME_HEIGHT * 2) + 0x02000000;

  // Use the ST7701Cached presto object, this works by providing the back_buffer it whould use to send to the display
  presto = new ST7701Cached(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0, SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT}, (uint16_t *)back_buffers[0]);

  // We use the other back_buffer for picographics.
  graphics = new PicoGraphics_PenRGB565(FRAME_WIDTH, FRAME_HEIGHT, (uint16_t *)back_buffers[1]);

  // set displayBuffer to the buffer currently being displayed
  uint8_t displayBuffer = 0;

  // Init the ST7701 display and clear back buffers
  presto->init();
  memset(back_buffers[0], 0, FRAME_WIDTH * FRAME_HEIGHT * 2);
  memset(back_buffers[1], 0, FRAME_WIDTH * FRAME_HEIGHT * 2);
  
  // inititalise pixels
  pixels.clear();
  for (int i = 0; i < BLOCK_COUNT; i++)
  {
    pt pixel;
    pixel.x = rand() % (graphics->bounds.w - PIX_WH);
    pixel.y = rand() % (graphics->bounds.h - PIX_WH);
    pixel.xOld = pixel.x;
    pixel.yOld = pixel.y;
    pixel.xVeryOld = pixel.x;
    pixel.yVeryOld = pixel.y;
    
    pixel.dx = float(rand() % 255) / 64.0f;
    pixel.dy = float(rand() % 255) / 64.0f;
    pixel.use_pen = graphics->create_pen(rand() % 255, rand() % 255, rand() % 255);
    pixels.push_back(pixel);
  }


  while (true)
  {
    float vsyncMs  = 0;
    float clearMs  = 0;
    float drawMs   = 0;
    float totalMs  = 0;
    float updateMs = 0;
    
    // Used for timings
    Elapsed elapsed;

    // update pixels
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
      if (pixel.x >= graphics->bounds.w - PIX_WH)
        pixel.dx *= -1;
      if (pixel.y < 0)
        pixel.dy *= -1;
      if (pixel.y >= graphics->bounds.h - PIX_WH)
        pixel.dy *= -1;
    }
    updateMs = elapsed.elapsedMs();

    // clear old pixels
    // CLEAR_TYPE 0 = no clear, 1 = partial clear, 2 = memset clear, 3 = picographics clear

#if CLEAR_TYPE == 1    
    graphics->set_pen(0);
    for(auto &pixel : pixels) {
      graphics->rectangle({(int32_t)pixel.xVeryOld, (int32_t)pixel.yVeryOld, PIX_WH, PIX_WH});
    }
#elif CLEAR_TYPE == 2
    memset(back_buffers[!displayBuffer], 0, FRAME_WIDTH * FRAME_HEIGHT * 2);
#elif CLEAR_TYPE == 3
    graphics->set_pen(0);
    graphics->clear();
#endif
    clearMs = elapsed.elapsedMs();

    // draw pixels
    for(auto &pixel : pixels) {
      graphics->set_pen(pixel.use_pen);
      graphics->rectangle({(int32_t)pixel.x, (int32_t)pixel.y, PIX_WH, PIX_WH});
    }
    drawMs = elapsed.elapsedMs();

    // swap back buffers
    displayBuffer = !displayBuffer;
    graphics->set_framebuffer(back_buffers[!displayBuffer]);
    presto->set_backbuffer(back_buffers[displayBuffer]);

    // wait for vsync, buffers are swapped here
    presto->wait_for_vsync();
    vsyncMs = elapsed.elapsedMs();


    totalMs = updateMs + clearMs + drawMs + vsyncMs;
    float fps = 1000.0f / totalMs;

    printf("U=%.2f ", updateMs);
    printf("C=%.2f ", clearMs);
    printf("D=%.2f ", drawMs);
    printf("V=%.2f ", vsyncMs);
    printf("A=%.2f ", totalMs);
    printf("F=%.2f\n", fps);
  }
}
