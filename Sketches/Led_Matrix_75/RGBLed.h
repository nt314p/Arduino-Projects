#ifndef RGBLed_h
#define RGBLed_h

#include "Arduino.h"

class RGBLed {
  public:
    RGBLed(byte r, byte g, byte b);
    void writeR(byte val);
    void writeB(byte val);
    void writeG(byte val);
    void writeRGB(byte r, byte g, byte b);
    void enable(bool state);
    bool isEnabled();
  private:
    void writeColor(char color, byte val);
    unsigned int rgb;
};

#endif
