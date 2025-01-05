#pragma once

class Elapsed
{
public:
  Elapsed()
  {
    last_time = time_us_64();
  }

  uint64_t elapsedUs(void)
  {
    uint64_t time_now = time_us_64();
    uint64_t elapsed = time_now - last_time;
    last_time = time_now;
    return elapsed;
  }

  float elapsedMs(void)
  {
    return elapsedUs() / 1000.0f;
  }

private:
  uint64_t last_time;
};
