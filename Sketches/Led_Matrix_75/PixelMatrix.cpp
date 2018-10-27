#include "Arduino.h"
#include "PixelMatrix.h"

PixelMatrix::PixelMatrix(byte pixData[]) {
  memcpy( pix, pixData, sizeof(pixData) );
}

byte PixelMatrix::getPixel(byte n) {
  byte shift = n % 4; // which section of the byte to read
  byte bytePos = (n - shift) / 4;
  byte section = pix[bytePos]; // the whole byte (1 of 9 in the array)
  byte val = bitRead(section, (3 - shift) * 2 + 1) * 2 + bitRead(section, (3 - shift));
  return val;
}
void PixelMatrix::setPixel(byte pos, byte state) {
  byte shift = pos % 4; // which section of byte to read
  byte bytePos = (pos - shift) / 4; // which index in the byte array to read (zero indexed)
  byte byteTarget = pix[bytePos];
  byte mask = 0b11000000 / pow(2, 2 * shift); // setting up byte mask (editing only two bits at a time)
  byteTarget = byteTarget & ~mask; // zeroing bits to recieve new values
  byteTarget += (state * pow(2, 2 * (3 - shift))); // shifting the new state over to correct space and adding it
  pix[bytePos] = byteTarget; // only 2 bits are replaced and the rest are kept
}
