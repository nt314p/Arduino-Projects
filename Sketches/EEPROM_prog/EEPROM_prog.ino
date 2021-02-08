// Some code taken from https://github.com/beneater/eeprom-programmer/blob/master/eeprom-programmer/eeprom-programmer.ino
// Credit goes to Ben Eater

// commands consist of a single char, followed by parameters (in hex) separated by commas. A semicolon ';' ends each command.
// READ: r[ADDRESS];
// WRITE: w[ADDRESS],[DATA];
// LOAD: l[START_ADDRESS],[DATA];
// DUMP: d[START_ADDRESS],[BYTES];
// PRINT: p[START_ADDRESS],[BYTES];

/*
   Print only works for start address values that are multiples of 16
*/

#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13
#define EEPROM_WORDS 32768

char cmdMode = '-';
unsigned int parameter = 0;
boolean onData = false;
unsigned int wData = 0;
boolean hasNibble = false; // used when loading values in, 
// false when 0 nibbles have been recieved, true when 1 nibble has been recieved (for data)

// Output the address bits and outputEnable signal using shift registers.
void setAddress(int address, bool outputEnable) {
  shiftOutFaster(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOutFaster(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  // pin 4 latch toggle
  PORTD &= ~_BV(PD4); // low
  PORTD |= _BV(PD4); // high
  PORTD &= ~_BV(PD4); // low

  //  digitalWrite(SHIFT_LATCH, LOW);
  //  digitalWrite(SHIFT_LATCH, HIGH);
  //  digitalWrite(SHIFT_LATCH, LOW);
}


// Read a byte from the EEPROM at the specified address.
byte readEEPROM(int address) {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) {
    pinMode(pin, INPUT);
  }

  setAddress(address, /*outputEnable*/ true);

  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
    data = (data << 1) + digitalRead(pin);
  }

  return data;
}

// Force write a byte to EEPROM (ignores write optimization where it checks for existing value)
void forceWriteEEPROM(int address, byte data) {
  setAddress(address, false);

  DDRD |= B11100000;
  DDRB |= B00011111;

  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }

  PORTB &= ~_BV(PB5); // low
  delayMicroseconds(1);
  PORTB |= _BV(PB5); // high
}


// Write a byte to the EEPROM at the specified address.
void writeEEPROM(int address, byte data) {
  if (readEEPROM(address) == data) return;

  setAddress(address, /*outputEnable*/ false);

  // set pins 5-12 to output
  /*
     05 PD5
     06 PD6
     07 PD7
     08 PB0
     09 PB1
     10 PB2
     11 PB3
     12 PB4

     DDRx(ABCD)
      bit 7 6 5 4 3 2 1 0

                    765xxxxx
     5-7   DDRD |= B11100000;
                    xxx43210
     8-12  DDRB |= B00011111;

      data bit  8  7  6  5  4  3  2  1
           pin 12 11 10  9  8  7  6  5

       writing 12-8, shift data over 3 bits right
       writing  7-5, shift data over 5 bits left

  */

  DDRD |= B11100000;
  DDRB |= B00011111;

  // set pins 5-12 to data
  //    data bit  8  7  6  5  4  3  2  1
  //         pin 12 11 10  9  8  7  6  5


  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }

  // pin 13 toggle write enable
  PORTB &= ~_BV(PB5); // low
  //digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(1);
  PORTB |= _BV(PB5); // high
  //digitalWrite(WRITE_EN, HIGH);
}

/* SOFTWARE WRITE PROTECTION DISABLE
  forceWriteEEPROM(0x5555, 0xAA);
  forceWriteEEPROM(0x2AAA, 0x55);
  forceWriteEEPROM(0x5555, 0x80);
  forceWriteEEPROM(0x5555, 0xAA);
  forceWriteEEPROM(0x2AAA, 0x55);
  forceWriteEEPROM(0x5555, 0x20);
*/

void setup() {

  // setting up pins
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);

  Serial.begin(500000);

//  delay(100);
//  forceWriteEEPROM(0x5555, 0xAA);
//  forceWriteEEPROM(0x2AAA, 0x55);
//  forceWriteEEPROM(0x5555, 0x80);
//  forceWriteEEPROM(0x5555, 0xAA);
//  forceWriteEEPROM(0x2AAA, 0x55);
//  forceWriteEEPROM(0x5555, 0x20);
}

void loop() {
  if (cmdMode == 'l') { // load
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == ';') {
        Serial.println("End of load");
        cmdMode = '-';
        parameter = 0;
        wData = 0;
        onData = false;
      } else if (input == ',') { // comma signals end of address
        onData = true;
      } else if (!onData) { // address
        parameter = parameter << 4;
        parameter = parameter | hexToInt(input);
      } else { // read data
        wData = wData << 4;
        wData = wData | hexToInt(input);
        if (hasNibble) { // we just loaded in the second nibble in the byte, write data
          writeEEPROM(parameter, wData);
          hasNibble = false;
          parameter++; // increment address
          wData = 0; // reset data
        } else { // we have just loaded in the first nibble, set nibble to true
          hasNibble = true;
        }
      }
    }
  } else if (cmdMode == '-') {
    if (Serial.available() > 0) { // check incoming chars for valid commands
      char input = Serial.read();

      if (input == 'r') {
        cmdMode = 'r';
        Serial.println("r mode");
      }
      if (input == 'w') {
        cmdMode = 'w';
        Serial.println("w mode");
      }
      if (input == 'l') {
        cmdMode = 'l';
        Serial.println("l mode");
      }
      if (input == 'd') {
        cmdMode = 'd';
        Serial.println("d mode");
      }
      if (input == 'p') {
        cmdMode = 'p';
        Serial.println("p mode");
      }
      if (input == 'e') {
        cmdMode = 'e';
        Serial.println("e mode");
      }
    }
  } else if (cmdMode == 'r') { // read
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input != ';') {
        parameter = parameter << 4;
        parameter = parameter | hexToInt(input);
      } else {
        byte result = readEEPROM(parameter);
        Serial.print("Read from ");
        Serial.print(parameter, HEX);
        Serial.print(" is ");
        Serial.println(result, HEX);
        cmdMode = '-';
        parameter = 0;
      }
    }
  } else if (cmdMode == 'w') { // write
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == ';') {
        writeEEPROM(parameter, wData);
        Serial.print("Wrote: ");
        Serial.print(wData, HEX);
        Serial.print(" to ");
        Serial.println(parameter, HEX);
        
        cmdMode = '-';
        parameter = 0;
        wData = 0;
        onData = false;
      } else if (input == ',') { // if we encounter a comma, we have reached the end of the address
        onData = true; //           and now read data
      } else if (!onData) { // address
        parameter = parameter << 4;
        parameter = parameter | hexToInt(input);
      } else { // data
        wData = wData << 4;
        wData = wData | hexToInt(input);
      }
    }
  } else  if (cmdMode == 'd') { // dump
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == ';') {
        for (int i = 0; i < wData; i++) { // iterate through the required number of bytes to dump
          int addr = parameter + i; // address of the row
          char byteLabel[2]; // buffer for byte to be printed
          sprintf(byteLabel, " %02X", readEEPROM(addr));
          Serial.print(byteLabel);
        }
        Serial.println();
        Serial.println("End of dump");
        cmdMode = '-';
        parameter = 0;
        wData = 0;
        onData = false;
      } else if (input == ',') { // comma signals the end of the start address, begin loading the number of bytes to dump
        onData = true;
      } else if (!onData) { // address
        parameter = parameter << 4;
        parameter = parameter | hexToInt(input);
      } else { // read data (in this case, the number of bytes to dump)
        wData = wData << 4;
        wData = wData | hexToInt(input); // todo: implement overloading of nibbles
      }
    }
  } else if (cmdMode == 'p') { // print
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == ';') {
        Serial.println("       0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F");

        for (int i = 0; i < wData; i += 16) { // iterate through the required number of bytes to print
          byte bytesInRow = ((wData - i) / 16 == 0) ? wData - i : 16; // normally 16, but last row might not have full 16
          int addr = parameter + i; // address of the row
          char addrLabel[4]; // buffer for address label
          char byteLabel[2]; // buffer for byte to be printed

          sprintf(addrLabel, "%04X ", addr);
          Serial.print(addrLabel);
          for (int j = 0; j < bytesInRow; j++) {
            sprintf(byteLabel, " %02X", readEEPROM(addr + j));
            Serial.print(byteLabel);
            if (j == 7)
              Serial.print(" "); // extra padding
          }
          Serial.println();
        }
        Serial.println("End of print");
        cmdMode = '-';
        parameter = 0;
        wData = 0;
        onData = false;
      } else if (input == ',') { // comma signals the end of the start address, begin loading the number of bytes to print
        onData = true;
      } else if (!onData) { // address
        parameter = parameter << 4;
        parameter = parameter | hexToInt(input);
      } else { // read data (in this case, the number of bytes to print)
        wData = wData << 4;
        wData = wData | hexToInt(input); // todo: implement overloading of nibbles
      }
    }
  } else if (cmdMode == 'e') { // erase
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == ';') {
        forceWriteEEPROM(0x5555, 0xAA);
        forceWriteEEPROM(0x2AAA, 0x55);
        forceWriteEEPROM(0x5555, 0x80);
        forceWriteEEPROM(0x5555, 0xAA);
        forceWriteEEPROM(0x2AAA, 0x55);
        forceWriteEEPROM(0x5555, 0x10);
        delay(20); // wait for chip erase
        Serial.println("Chip erased");
        cmdMode = '-';
      }
    }
  }
}

int hexToInt(char hex) {
  char str[] = {hex};
  char *ptr;
  return strtoul(str, &ptr, 16);
}

void shiftOutFaster(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val) {
  uint8_t i;

  for (i = 0; i < 8; ++i)  {
    if (!!(val & (1 << (7 - i))))
      PORTD |= _BV(PD2); // data high
    else
      PORTD &= ~_BV(PD2); // data low

    PORTD |= _BV(PD3); // pin 3 high (clock)
    PORTD &= ~_BV(PD3); // pin 3 low
  }
}
