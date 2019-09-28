#include "Arduino.h"
#include "Timer.h"

Timer::Timer(void (*f)(), unsigned int ival, bool e) {
  pfunc = *f;
  interval = ival;
  enabled = e;
  prev_ms = 0;
}

void Timer::refresh(unsigned long curr_ms) {
  // difference between last run time is greater than interval
  if (curr_ms - prev_ms >= interval && enabled) {
    pfunc();
    prev_ms = curr_ms;
  }
}
void Timer::setInterval(unsigned int newInterval) {
  interval = newInterval;
  return interval;
}
