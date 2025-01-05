# Examples for using the st7701Cached class

  ST7701Cached uses a small cache of pico ram in order to send the
  data to the display, this saves pico memory as a whole framebuffers
  worth of data is not needed. Instead it uses 2880 bytes of memory
  for its cached data.

  The original ST7701 class works by only updateing the rows of the screen 
  that are not being currently sent out to the display when you call Update(), 
  this requires a whole frame buffer of data in the pico ram, and cleverly 
  enables you to use a single back buffer and get glitch free graphics.

  As ST7701Cached is only using a small amount of pico memory it cannot
  work in the same way, you don't even need to call Update() as it is constantly
  sending the current back buffer out to the display.

## SinglePsramBuffer480x480.cpp

  This example shows using a single back buffer stored in PSRAM with
  the Presto running at 480x480 and using the ST7701Cached class.

  If you are running as in this example only using a single back buffer
  then you can see glitching whenever your code is updating the same row of the 
  display that is currently being sent to the screen.

  To avoid this glitching you can use two back buffers but for this example
  one back buffer works fine.

  Note: Timings and fps data are logged to the USB UART, if you attach a 
        serial monitor to see them then you may get some slight glitching
        with the display.

## DoublePsramBuffer480x480.cpp

  This example shows two back buffers stored in PSRAM with
  the Presto running at 480x480 and using the ST7701Cached class.

  To get glitch free graphics you need to use two back buffers and swap between them
  This means you need to keep these back buffers in sync or cleared, the psram is pretty
  slow on the presto, so clearing or copying a buffer every frame or can be quite costly.

  This example bounces some boxes around, there are different clear stratagies that
  you can set using the define CLEAR_TYPE to see the speed differences.

  Note: Timings and fps data are logged to the USB UART, if you attach a 
        serial monitor to see them then you may get some slight glitching
        with the display.
