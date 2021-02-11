// Some code taken from https://github.com/beneater/eeprom-programmer/blob/master/eeprom-programmer/eeprom-programmer.ino
// Credit goes to Ben Eater

// Timings
// setAddress: 40-44 us
// write (read + force write): 200-208 us OUTDATED
// read: 76-84 us
// Serial.println(micros() - t); takes 84-88 us!
// load cycle (write + serial.println): 276-284 us (too fast for EEPROM to keep up) OUTDATED
// Remember, just because the write *function* completes in 200 us does not mean that the
// EEPROM writes the data and is ready for more in 200 us.

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

unsigned long t = micros();
const char validCommands[] = { 'r', 'w', 'l', 'd', 'p', 'e' };
char cmdMode = '-';
unsigned int parameter = 0;
boolean onData = false;
unsigned int wData = 0;
boolean hasNibble = false; // used when loading values in,
// false when 0 nibbles have been recieved, true when 1 nibble has been recieved (for data)

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
  if (Serial.available() > 0) {
    char input = Serial.read();

    if (cmdMode == 'l') { // load
      if (input == ';') {
        Serial.println(F("End of load"));

        resetCmdVars();
      } else if (input == ',') { // comma signals end of address
        onData = true;
      } else if (!onData) { // address
        processParameter(input);
      } else { // read data
        processWData(input);
        if (hasNibble) { // we just loaded in the second nibble in the byte, write data
          writeEEPROM(parameter, wData);
          Serial.println(micros() - t);
          t = micros();
          hasNibble = false;
          parameter++; // increment address
          wData = 0; // reset data
        } else { // we have just loaded in the first nibble, set nibble to true
          hasNibble = true;
        }
      }
    } else if (cmdMode == '-') { // default
      setCmdMode(input);
    } else if (cmdMode == 'r') { // read

      if (input != ';') {
        processParameter(input);
      } else {
        unsigned long t = micros();
        byte result = readEEPROM(parameter);
        Serial.println(micros() - t);
        Serial.print("Read from ");
        Serial.print(parameter, HEX);
        Serial.print(" is ");
        Serial.println(result, HEX);

        resetCmdVars();
      }
    } else if (cmdMode == 'w') { // write

      if (input == ';') {
        unsigned long t = micros();
        writeEEPROM(parameter, wData);
        unsigned long diff = micros() - t;
        Serial.print("Wrote: ");
        Serial.print(wData, HEX);
        Serial.print(" to ");
        Serial.println(parameter, HEX);
        Serial.println(diff);

        resetCmdVars();
      } else if (input == ',') { // if we encounter a comma, we have reached the end of the address
        onData = true; //           and now read data
      } else if (!onData) { // address
        processParameter(input);
      } else { // data
        processWData(input);
      }
    } else  if (cmdMode == 'd') { // dump
      if (input == ';') {
        for (int i = 0; i < wData; i++) { // iterate through the required number of bytes to dump
          int addr = parameter + i; // address of the row
          char byteLabel[2]; // buffer for byte to be printed
          sprintf(byteLabel, " %02X", readEEPROM(addr));
          Serial.print(byteLabel);
        }
        Serial.println(F("\nEnd of dump"));

        resetCmdVars();
      } else if (input == ',') { // comma signals the end of the start address, begin loading the number of bytes to dump
        onData = true;
      } else if (!onData) { // address
        processParameter(input);
      } else { // read data (in this case, the number of bytes to dump)
        processWData(input); // todo: implement overloading of nibbles
      }
    } else if (cmdMode == 'p') { // print
      cmdPrint(input);

    } else if (cmdMode == 'e') { // erase
      if (input == ';') {
        forceWriteEEPROM(0x5555, 0xAA);
        forceWriteEEPROM(0x2AAA, 0x55);
        forceWriteEEPROM(0x5555, 0x80);
        forceWriteEEPROM(0x5555, 0xAA);
        forceWriteEEPROM(0x2AAA, 0x55);
        forceWriteEEPROM(0x5555, 0x10);
        delay(20); // wait for chip erase

        Serial.println(F("Chip erased"));
        resetCmdVars();
      }
    }
  }
}

void cmdPrint(char input) {
  if (input == ';') {
    Serial.println(F("       0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F"));

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

    resetCmdVars();
  } else if (input == ',') { // comma signals the end of the start address, begin loading the number of bytes to print
    onData = true;
  } else if (!onData) { // address
    processParameter(input);
  } else { // read data (in this case, the number of bytes to print)
    processWData(input); // todo: implement overloading of nibbles
  }
}

void setCmdMode(char input) {
  for (byte i = 0; i < sizeof(validCommands); ++i) {
    if (input == validCommands[i]) {
      cmdMode = input;
      Serial.print(input);
      Serial.println(F(" mode"));
    }
  }
}

void resetCmdVars() {
  cmdMode = '-';
  parameter = 0;
  wData = 0;
  onData = false;
}

void processParameter(char input) {
  parameter = parameter << 4;
  parameter = parameter | hexToInt(input);
}

void processWData(char input) {
  wData = wData << 4;
  wData = wData | hexToInt(input);
}

// Output the address bits and outputEnable signal using shift registers.
void setAddress(int address, bool outputEnable) {
  shiftOutFaster(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOutFaster(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  // pin 4 latch toggle
  PORTD &= ~_BV(PD4); // low
  PORTD |= _BV(PD4); // high
  PORTD &= ~_BV(PD4); // low
}


// Read a byte from the EEPROM at the specified address.
byte readEEPROM(int address) {
  DDRD &= B00011111; // PD5, PD6, PD7 inputs (low), pins 5, 6, 7
  DDRB &= B11100000; // PB0, PB1, PB2, PB3, PB4 inputs, pins 8, 9, 10, 11, 12

  setAddress(address, /*outputEnable*/ true);

  // MSB is pin 12, LSB is pin 5
  //                          12---8                    7-5
  byte portRead = ((PINB & B00011111) << 3) + ((PIND & B11100000) >> 5);

  //  Serial.println("********");
  //  Serial.println(PINB, BIN);
  //  Serial.println(PIND, BIN);
  //  Serial.println(portRead, BIN);
  //
  //  byte data = 0;
  //
  //  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
  //    data = (data << 1) + digitalRead(pin);
  //  }
  //
  //  Serial.println(data, BIN);

  return portRead;
}

// Force write a byte to EEPROM (ignores write optimization where it checks for existing value)
void forceWriteEEPROM(int address, byte data) {
  setAddress(address, false);

  DDRD |= B11100000;
  DDRB |= B00011111;

  PORTD &= B00011111; // clear bits
  PORTB &= B11100000;
  PORTD |= data << 5; // write bits 2,1,0
  PORTB |= data >> 3; // write bits 7,6,5,4,3

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

  //  Serial.println(DDRB, BIN);
  //  Serial.println(DDRD, BIN);

  DDRD |= B11100000;
  DDRB |= B00011111;

  // INSERT DELAY HERE, CAUSING MISSED WRITES!!

  //  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
  //    pinMode(pin, OUTPUT);
  //  }
  //
  //  Serial.println(DDRB, BIN);
  //  Serial.println(DDRD, BIN);

  // set pins 5-12 to data
  //    data bit  8  7  6  5  4  3  2  1
  //         pin 12 11 10  9  8  7  6  5

  // bit number 7654320
  //    210
  // D: ddd00000
  //       76543
  // B: 000ddddd
  
  PORTD &= B00011111; // clear bits
  PORTB &= B11100000;
  PORTD |= data << 5; // write bits 2,1,0
  PORTB |= data >> 3; // write bits 7,6,5,4,3

  Serial.println(PORTD, BIN);
  Serial.println(PORTB, BIN);
  //
  //  delayMicroseconds(4);
  //  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
  //    digitalWrite(pin, data & 1);
  //    data = data >> 1;
  //  }

  // pin 13 toggle write enable
  PORTB &= ~_BV(PB5); // low
  //digitalWrite(WRITE_EN, LOW);
  //delayMicroseconds(1);
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
