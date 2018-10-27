#ifndef Timer_h
#define Timer_h

#include "Arduino.h"

class Timer {
  public:
    Timer(void (*f)(), unsigned int ival, bool e);
    unsigned int interval; // triggers the function every x ms (max 65,535 ms or 65.5 seconds)
    bool enabled; // should the timer be running?
    void refresh(unsigned long curr_ms); // input current ms and see if the timer needs to run

  private:
    int (*pfunc)();
    unsigned long prev_ms; // comparison ms
};

#endif
