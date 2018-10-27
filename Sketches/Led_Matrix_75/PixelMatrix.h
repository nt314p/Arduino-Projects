#ifndef PixelMatrix_h
#define PixelMatrix_h

#include "Arduino.h"

// Pixel Matrix class
class PixelMatrix {
  public:
    PixelMatrix(byte pixData[]);
    byte pix[];
    byte getPixel(byte n);
    void setPixel(byte pos, byte state);
};

#endif
