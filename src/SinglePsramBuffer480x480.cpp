// ******************************************************************************
// This example shows using a single back buffer stored in PSRAM with
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
// So if you are running as in this example only using a single back buffer
// then you can see glitching whenever your code is updating the same row of the 
// display that is currently being sent to the screen.
//
// To avoid this glitching you can use two back buffers but for this example
// one back buffer works fine.
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

FT6236 touchDisplay;

uint16_t                *back_buffer; // Single back buffer to use
ST7701Cached            *presto;      // Sends data to the display
PicoGraphics_PenRGB565  *graphics;    // We draw with this

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

  // allocate 480x480 back buffer in psram, use uncached address
  back_buffer = (uint16_t *)ps.Malloc(FRAME_WIDTH * FRAME_HEIGHT * 2) + 0x02000000;

  // Use the ST7701Cached presto object, this works by providing the back_buffer it whould use to send to the display
  presto = new ST7701Cached(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0, SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT}, (uint16_t *)back_buffer);

  // We use the same back_buffer for picographics.
  graphics = new PicoGraphics_PenRGB565(FRAME_WIDTH, FRAME_HEIGHT, (uint16_t *)back_buffer);

  // Init the ST7701 display and clear back_buffer
  presto->init();
  memset(back_buffer, 0, FRAME_WIDTH * FRAME_HEIGHT * 2);

  // display some help text
  graphics->set_pen(0xffff);
  graphics->text("Draw with finger, tap with two fingers one above the other to clear.", {0, 0}, 480);

  // Variables for changing radius
  int16_t radius       = 10;
  int16_t radiusChange = 1;
  
  // variables for chaning colors
  int16_t colorComponents[3] = {(int16_t)(rand()%256), (int16_t)(rand()%256), (int16_t)(rand()%256)};
  int8_t  colorComponentsChange[3] = {(int8_t)((rand()%5)-2), (int8_t)((rand()%5)-2), (int8_t)((rand()%5)-2)};
  
  while (true)
  {
    float vsyncMs = 0;
    float touchMs = 0;
    float drawMs = 0;
    float totalMs = 0;
    
    // Used for timings
    Elapsed elapsed;

    // poll for touches
    touchDisplay.ReadTouch();
    touchMs = elapsed.elapsedMs();

    // wait for vsync
    presto->wait_for_vsync();
    vsyncMs = elapsed.elapsedMs();

    // draw touch
    const FT6236::Touch &touch0 = touchDisplay.GetTouch(0);
    touchMs = elapsed.elapsedMs();

    if(touch0.active && touch0.HasMoved())
    {
      // Next radius
      if(radius > 50 || radius < 10)
        radiusChange = 0 - radiusChange;
      radius += radiusChange;

      // calc color
      for(int i = 0; i < 3; i++)
      {
        colorComponents[i] += colorComponentsChange[i];
        if(colorComponents[i] > 255)
        {
          colorComponents[i] = 255 - (colorComponents[i]-255);
          colorComponentsChange[i] = 0 - colorComponentsChange[i];
        }

        if(colorComponents[i] < 0 )
        {
          colorComponents[i] = 0 - colorComponents[i];
          colorComponentsChange[i] = 0 - colorComponentsChange[i];
        }
      }

      // draw circles
      graphics->set_pen(colorComponents[0], colorComponents[1], colorComponents[2]);
      graphics->circle({touch0.x, touch0.y}, radius);
      graphics->circle({FRAME_WIDTH-touch0.x, touch0.y}, radius);
      graphics->circle({FRAME_WIDTH-touch0.x, FRAME_HEIGHT-touch0.y}, radius);
      graphics->circle({touch0.x, FRAME_HEIGHT-touch0.y}, radius);
    }

    // if we have a second touch then clear the screen and start again
    const FT6236::Touch &touch1 = touchDisplay.GetTouch(1);
    if(touch1.active)
    {
      // generate new colors
      for(int i = 0; i < 3; i++)
      {
        colorComponents[i] = (int16_t)rand()%256;
        colorComponentsChange[i] = (int16_t)(rand()%11)-5;
      }

      // clear the back_buffer
      memset(back_buffer, 0, FRAME_WIDTH * FRAME_HEIGHT * 2);
   }

    drawMs = elapsed.elapsedMs();

    totalMs = touchMs + drawMs + vsyncMs;
    float fps = 1000.0f / totalMs;

    printf("V=%.2f ", vsyncMs);
    printf("T=%.2f ", touchMs);
    printf("D=%.2f ", drawMs);
    printf("A=%.2f ", totalMs);
    printf("F=%.2f\n", fps);
  }
}
