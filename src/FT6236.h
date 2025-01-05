#pragma once

#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define TOUCH_INT   (32)
#define TOUCH_I2C   (1)
#define TOUCH_SDA   (30)
#define TOUCH_SCL   (31)
#define TOUCH_ADDR  (0x48)

#define STATE_DOWN    (0b00)
#define STATE_UP      (0b01)
#define STATE_CONTACT (0b10)
#define STATE_NONE    (0b11)

// FT6236U

class FT6236
{
public:
  struct Touch
  {
    int16_t   id;
    bool      active;
    int16_t   x;
    int16_t   y;
    int16_t   dx;
    int16_t   dy;

    Touch(void) : id(0), active(false), x(0), y(0), dx(0), dy(0) {};

    bool HasMoved(void) const
    {
      return (dx != 0) || (dy !=0);
    }
  };

  typedef enum
  {
    regDevMode    = 0x00,
    regGestureId  = 0x01,
    regThreshold  = 0x80,
    regFilter     = 0x85,
    regChipId     = 0xA3,
  } Reg;

  FT6236(void)
  {
    i2c_init(i2c1, pimoroni::I2C_DEFAULT_BAUDRATE);
    
    gpio_set_function(TOUCH_SDA, GPIO_FUNC_I2C); 
    gpio_pull_up(TOUCH_SDA);
    
    gpio_set_function(TOUCH_SCL, GPIO_FUNC_I2C); 
    gpio_pull_up(TOUCH_SCL);
  }

  ~FT6236(void)
  {
    i2c_deinit(i2c1);
    
    gpio_set_function(TOUCH_SDA, GPIO_FUNC_NULL);
    gpio_disable_pulls(TOUCH_SDA);

    gpio_set_function(TOUCH_SCL, GPIO_FUNC_NULL);
    gpio_disable_pulls(TOUCH_SCL);
  }

  void WriteReg(Reg reg, uint8_t val)
  {
    uint8_t data[] = {reg, val};
    i2c_write_blocking(i2c1, TOUCH_ADDR, data, 2, true);
  }

  uint8_t ReadReg(Reg reg)
  {
    uint8_t result = 0;

    i2c_write_blocking(i2c1, TOUCH_ADDR, (uint8_t *)&reg, 1, true);
    i2c_read_blocking(i2c1, TOUCH_ADDR, &result, 1, false);
    return result;
  }

  uint8_t ReadTouch(void)
  {
    //SetFilter(255);
    //uint8_t uFilter = GetFilter();

    uint8_t buffer[16];
    uint8_t reg = 0;

    i2c_write_blocking(i2c1, TOUCH_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c1, TOUCH_ADDR, buffer, 16, false);

    int touchCount = buffer[2];
    touches[0].active = false;
    touches[1].active = false;

    if(touchCount)
    {
      for (uint8_t i = 0; i < 2; i++)
      {
        int16_t uId = buffer[0x05 + i * 6] >> 4;

        if(uId < 3)
        {
          int16_t x = ((buffer[0x03 + i * 6] & 0x0F) << 8) | buffer[0x04 + i * 6];
          int16_t y =  ((buffer[0x05 + i * 6] & 0x0F) << 8) | buffer[0x06 + i * 6];
          touches[uId].dx = x - touches[uId].x;
          touches[uId].dy = y - touches[uId].y;
          touches[uId].x  = x;
          touches[uId].y  = y;
          touches[uId].active = true;
        }
      }
    }
    return(touchCount);
  }

  const Touch &GetTouch(uint8_t id) const
  {
    if(id < 3)
      return touches[id];
    else
      return nullTouch;
  }

private:
   Touch touches[2];
   Touch nullTouch;
};