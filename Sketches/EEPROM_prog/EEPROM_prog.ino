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
// READ: r[ADDRESS];         // 4 = 4 bytes
// WRITE: w[ADDRESS],[DATA]; // 4 + 1 + 2 = 7 bytes
// LOAD: l[START_ADDRESS],[DATA]; // cannot buffer parameters
// DUMP: d[START_ADDRESS],[#BYTES (hex)]; //  4 + 1 + 4 = 9 bytes
// PRINT: p[START_ADDRESS],[#BYTES (hex)]; // 4 + 1 + 4 = 9 bytes

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
#define MAX_BUFFER_LENGTH 900
#define MAX_PARAM_BUFFER_LENGTH 9

char dataBuffer[MAX_BUFFER_LENGTH];
char parameterBuffer[MAX_PARAM_BUFFER_LENGTH];
byte paramIndex = 0;

bool hasCommand = false;
char currentCommand = ' ';

unsigned long t = micros();
const char validCommands[] = { 'r', 'w', 'l', 'd', 'p', 'e' };
char cmdMode = '-';
unsigned int parameter = 0;
bool onData = false;
unsigned int wData = 0;
bool hasNibble = false; // used when loading values in,
// false when 0 nibbles have been recieved, true when 1 nibble has been recieved (for data)

unsigned int bufferLength = 0;
unsigned int readIndex = 0;
unsigned int writeIndex = 0;

char readBuffer() {
  if (bufferLength <= 0) {
    return -1;
  }
  char value = dataBuffer[readIndex];
  bufferLength--;
  readIndex++;
  if (readIndex >= MAX_BUFFER_LENGTH) {
    readIndex = 0;
  }
  return value;
}

void writeBuffer(char value) {
  if (bufferLength >= MAX_BUFFER_LENGTH) {
    return;
  }
  dataBuffer[writeIndex] = value;
  bufferLength++;
  writeIndex++;
  if (writeIndex >= MAX_BUFFER_LENGTH) {
    writeIndex = 0;
  }
}

void setup() {

  // setting up pins
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);

  Serial.begin(38400);

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
    if (bufferLength < MAX_BUFFER_LENGTH) {
      writeBuffer(input);
    } else {
      Serial.println(F("Buffer overflow!"));
    }
  }
  old_loop();
}

void old_loop() {
  if (bufferLength > 0) {
    char input = readBuffer();

    if (!hasCommand) {
      if (isValidCommand(input)) {
        currentCommand = input;
        hasCommand = true;
        Serial.println(F("Command is valid!"));
        return;
      }
      Serial.println(F("Invalid command!"));
    }

    if (hasCommand && currentCommand != 'l') {
      if (input == ';') {
        evaluateCommand();
        finalizeCommand();
      } else {
        parameterBuffer[paramIndex] = input;
        paramIndex++;
        if (paramIndex > MAX_PARAM_BUFFER_LENGTH) {
          Serial.println(F("Invalid command length"));
        }
      }
    }
    return;

    // -------------

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

void commandPrint(int address, int numBytes) {
  Serial.println(F("       0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F"));

  char addrLabel[4]; // buffer for address label
  char byteLabel[2]; // buffer for byte to be printed

  for (int i = 0; i < numBytes; i += 16) { // iterate through the required number of bytes to print
    byte bytesInRow = ((numBytes - i) / 16 == 0) ? numBytes - i : 16; // normally 16, but last row might not have full 16
    int addr = address + i; // address of the row

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

void evaluateCommand() {
  byte commaIndex = 255;
  for (byte i = 0; i < paramIndex; i++) {
    if (parameterBuffer[i] == ',') {
      commaIndex = i;
    }
  }
  if (commaIndex == 255) {
    if (currentCommand == 'r') { // command is read, takes only single parameter
      commaIndex = paramIndex;
    } else {
      Serial.println(F("Incorrect parameter number"));
      return;
    }
  }
  int address = parseIntParam(0, commaIndex); // start of buffer to comma
  int dataParam = parseIntParam(commaIndex + 1, paramIndex); // after comma to end of buffer
  Serial.println(commaIndex);
  Serial.println(paramIndex);
  Serial.print("Address: ");
  Serial.println(address);
  Serial.print("Data: ");
  Serial.println(dataParam);
  Serial.println(currentCommand);

  if (currentCommand == 'w') {
    Serial.println(F("cmd is w!"));
  }

  switch (currentCommand) {
    case 'r':
      //Serial.println(F("got r"));
      // t = micros();
      byte result=10;// = readEEPROM(address);
      //      Serial.println(micros() - t);
      //      Serial.print("Read from ");
      //      Serial.print(address, HEX);
      //      Serial.print(" is ");
      //Serial.println(result, HEX);
      break;
    case 'w':
      Serial.println(F("got w"));
      break;
    case 'd':
      Serial.println(F("got d"));
      break;
  }

  switch (currentCommand) {
    case 'x':
      t = micros();
      byte result = readEEPROM(address);
      Serial.println(micros() - t);
      Serial.print("Read from ");
      Serial.print(address, HEX);
      Serial.print(" is ");
      Serial.println(result, HEX);
      break;
    case 'w':
      Serial.println(F("Writing..."));
      t = micros();
      writeEEPROM(address, (byte) dataParam);
      unsigned long diff = micros() - t;
      Serial.print("Wrote: ");
      Serial.print(dataParam, HEX);
      Serial.print(" to ");
      Serial.println(address, HEX);
      Serial.println(diff);
      break;
    case 'p':
      Serial.println("Printing...");
      commandPrint(address, dataParam);
      break;
    case 'd':
      Serial.println("Dumping...");
      char byteLabel[2]; // buffer for byte to be printed
      for (int i = 0; i < dataParam; i++) { // iterate through the required number of bytes to dump
        int addr = address + i; // address of the row
        sprintf(byteLabel, " %02X", readEEPROM(addr));
        Serial.print(byteLabel);
      }
      Serial.println(F("\nEnd of dump"));
      break;
    case 'e':
      forceWriteEEPROM(0x5555, 0xAA);
      forceWriteEEPROM(0x2AAA, 0x55);
      forceWriteEEPROM(0x5555, 0x80);
      forceWriteEEPROM(0x5555, 0xAA);
      forceWriteEEPROM(0x2AAA, 0x55);
      forceWriteEEPROM(0x5555, 0x10);
      delay(20); // wait for chip erase

      Serial.println(F("Chip erased"));
      break;
  }
}

void finalizeCommand() { // resets command variables
  for (byte i = 0; i < MAX_PARAM_BUFFER_LENGTH; ++i) {
    parameterBuffer[i] = 0;
  }
  paramIndex = 0;
  hasCommand = false;
}

int parseIntParam(byte startIndex, byte endIndex) {
  int value = 0;
  for (byte i = startIndex; i < endIndex; i++) {
    value = value << 4;
    value += hexToByte(parameterBuffer[i]);
  }
  return value;
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

bool isValidCommand(char input) {
  for (byte i = 0; i < sizeof(validCommands); ++i) {
    if (input == validCommands[i])
      return true;
  }
  return false;
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
  if (readEEPROM(address) == data) {
    Serial.println("skipped!");
    return;
  }

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

byte hexToByte(char hex) {
  if (hex >= 97) { // lowercase a-f
    hex -= 32; // capitalize
  }
  if (48 <= hex && hex <= 57) { // 0 - 9
    return hex - 48;
  }
  if (65 <= hex && hex <= 70) { // A - F
    return hex - 55;
  }
  return -1;
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
