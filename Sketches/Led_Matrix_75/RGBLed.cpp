#include "Arduino.h"
#include "RGBLed.h"

RGBLed::RGBLed(byte r, byte g, byte b) {
  unsigned int rgb = 0b1000000000000000;

  /* Memory Structure of rgb
   * Bit 15 ----- Enabled bit
   * Bit 14-10 -- 5 bit red value
   * Bit 9-5 ---- 5 bit green value
   * Bit 4-0 ---- 5 bit blue value
   */
}

void RGBLed::writeR(byte val) {
  writeColor('r', val);  
}

void RGBLed::writeG(byte val) {
  writeColor('g', val);  
}

void RGBLed::writeB(byte val) {
  writeColor('b', val);  
}

void RGBLed::enable(bool state) {
  bitWrite(rgb, 15, state); // writing the bit to the leftmost bit
}

bool RGBLed::isEnabled () {
  return bitRead(rgb, 15); // read the leftmost bit
}

void RGBLed::writeColor(char color, byte val) {
  byte shift = 0; // how many bits left to shift
  switch (color) { // selecting the shift based on color
    case 'r':
      shift = 10;
    case 'g':
      shift = 5;
    case 'b':
      shift = 0;
  }

  int mask = 0b0000000000011111;
  int b = val & mask; // chopping off 3 bits to make it 5
  b = b << shift; // shifting over the bits to write
  mask = mask << shift; // shifting the mask over shift bits
  rgb = rgb & !mask; // clearing the section
  rgb = rgb | b; // OR-ing the rgb (effectively inserts the bits b into rgb)
}


