#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13
#define EEPROM_WORDS 32768
#define ERASE_CONFIRMATION 0xBEEF
#define RING_BUFFER_SIZE 256

enum State {
  Idle,
  Address,
  Parameter
};

enum Command {
  None,
  Read,
  Write,
  Load,
  Dump,
  Print,
  Erase
};

const char validCommands[] = { 'r', 'w', 'l', 'd', 'p', 'e' };
const Command enumCommands[] = { Command::Read, Command::Write, Command::Load, Command::Dump, Command::Print, Command::Erase };
unsigned int address = 0;
unsigned int parameter = 0;
unsigned int byteCount = 0;

State currentState = State::Idle;
Command currentCommand = Command::None;

// Command consists of a single identifier char followed by [PARAMETER(byte size)] in byte form
// READ: r[ADDRESS(2)] // 2 bytes
// WRITE: w[ADDRESS(2)][DATA(1)] // 3 bytes
// LOAD: l[START_ADDRESS(2)][COUNT(2)][DATA(COUNT)] // 4 + COUNT bytes
// DUMP: d[START_ADDRESS(2)][COUNT(2)] // 4 bytes
// PRINT: p[START_ADDRESS(2)][COUNT(2)] // 4 bytes
// ERASE: e[ADDRESS(2)] // 2 bytes, note: the address must equal BEEF for successful erase (accidental erase prevention)

// 252 bytes/s write
// 22.9 kilobytes/s read

void setup() {
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(WRITE_EN, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  digitalWrite(SHIFT_CLK, LOW);
  digitalWrite(SHIFT_LATCH, LOW);

  Serial.begin(250000);
}

void loop() {  
  if (!Serial.available()) return;

  byte input = Serial.read();
  
  if (currentState == State::Idle) { // input is command char   
    bool isValidCommand = false;
    for (int i = 0; i < sizeof(validCommands); i++) {
      if (validCommands[i] != (char) input) continue;

      currentCommand = enumCommands[i];
      isValidCommand = true;
      break;
    }

    if (!isValidCommand) {
      Serial.print("Invalid command: ");
      Serial.println((char) input);
      resetState();
      return;
    }

    currentState = State::Address;
  } else if (currentState == State::Address) {
    if (byteCount == 0) {
      address += input; // read in upper byte
      address <<= 8;
      byteCount++;
      return;
    } else if (byteCount == 1) {
      address += input; // read in lower byte
      byteCount++;

      if (currentCommand == Command::Read) {
        Serial.write(readEEPROM(address));
        resetState();
        return;
      } else if (currentCommand == Command::Erase) {
        if (address != ERASE_CONFIRMATION) {
          Serial.write(0); // confirmation did not match
        } else {
          eraseEEPROM();
          Serial.write(1);
        }
        resetState();
        return;
      }

      currentState = State::Parameter;
      return;
    }
  } else if (currentState == State::Parameter) {
    if (byteCount == 2) {
      parameter += input;
      byteCount++;
      if (currentCommand == Command::Write) {
        smartWriteEEPROM(address, parameter);
        Serial.write(Serial.available());
        resetState();
      }
      return;
    } else if (byteCount == 3) {
      parameter <<= 8;
      parameter += input;
      byteCount++;

      if (currentCommand == Command::Dump) {
        for (int i = 0; i < parameter; i++) {
          Serial.write(readEEPROM(address + i));
        }
        resetState();
      } else if (currentCommand == Command::Print) {
        commandPrint(address, parameter);
        resetState();
      }
      return;
    }

    if (currentCommand != Command::Load) return;

    smartWriteEEPROM(address, input);
    Serial.write(Serial.available());
    address++;
    byteCount++;

    if (byteCount >= parameter + 4) {
      resetState();
      return;
    }
  }
}

void resetState() {
  byteCount = 0;
  address = 0;
  parameter = 0;
  currentState = State::Idle;
  currentCommand = Command::None;
}

void commandPrint(unsigned int address, unsigned int numBytes) {
  Serial.println(F("       0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F"));

  char addrLabel[4]; // buffer for address label
  char byteLabel[2]; // buffer for byte to be printed

  for (unsigned int i = 0; i < numBytes; i += 16) { // iterate through the required number of bytes to print
    byte bytesInRow = ((numBytes - i) / 16 == 0) ? numBytes - i : 16; // normally 16, but last row might not have full 16
    int addr = address + i; // address of the row

    sprintf(addrLabel, "%04X ", addr);
    Serial.print(addrLabel);
    for (int j = 0; j < bytesInRow; j++) {
      sprintf(byteLabel, " %02X", readEEPROM(addr + j));
      Serial.print(byteLabel);
      if (j == 7)
        Serial.print(" ");
    }
    Serial.println();
  }
}

void eraseEEPROM() {
  forceWriteEEPROM(0x5555, 0xAA);
  forceWriteEEPROM(0x2AAA, 0x55);
  forceWriteEEPROM(0x5555, 0x80);
  forceWriteEEPROM(0x5555, 0xAA);
  forceWriteEEPROM(0x2AAA, 0x55);
  forceWriteEEPROM(0x5555, 0x10);
  delay(20); // wait for chip erase
  disableWriteProtection();
}

void disableWriteProtection() {
  forceWriteEEPROM(0x5555, 0xAA);
  forceWriteEEPROM(0x2AAA, 0x55);
  forceWriteEEPROM(0x5555, 0x80);
  forceWriteEEPROM(0x5555, 0xAA);
  forceWriteEEPROM(0x2AAA, 0x55);
  forceWriteEEPROM(0x5555, 0x20);
  delay(1);
}

// Read a byte from the EEPROM at the specified address
byte readEEPROM(unsigned int address) {
  // set data pins direction as input
  DDRD &= B00011111; // PD5, PD6, PD7 inputs (low), pins 5, 6, 7
  DDRB &= B11100000; // PB0, PB1, PB2, PB3, PB4 inputs, pins 8, 9, 10, 11, 12

  setAddress(address, true);
  
  // MSB is pin 12, LSB is pin 5
  //                 12---8                    7-5
  return ((PINB & B00011111) << 3) + ((PIND & B11100000) >> 5);
}

// Writes a value and then waits until the data read back is valid
void smartWriteEEPROM(unsigned int address, byte data) {
  if (readEEPROM(address) == data) return;

  forceWriteEEPROM(address, data);
  while (readEEPROM(address) != data);
}

// Force write a byte to EEPROM (ignores write optimization where it checks for existing value)
void forceWriteEEPROM(unsigned int address, byte data) {
  setAddress(address, false);

  // set data pins direction as ouput
  DDRD |= B11100000;
  DDRB |= B00011111;

  PORTD &= B00011111; // clear bits
  PORTB &= B11100000;
  PORTD |= data << 5; // write bits 2,1,0
  PORTB |= data >> 3; // write bits 7,6,5,4,3

  PORTB &= ~_BV(PB5); // low
  PORTB |= _BV(PB5); // high
}

// Write a byte to the EEPROM at the specified address
void writeEEPROM(unsigned int address, byte data) {
  if (readEEPROM(address) == data) return;
  forceWriteEEPROM(address, data);
}

// Output the address bits and outputEnable signal using shift registers.
void setAddress(unsigned int address, bool outputEnable) {
  shiftOutFaster(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOutFaster(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  // pin 4 latch toggle
  PORTD |= _BV(PD4); // high
  PORTD &= ~_BV(PD4); // low
}

void shiftOutFaster(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val) {
  for (byte i = 0; i < 8; ++i)  {
    if (!!(val & (1 << (7 - i))))
      PORTD |= _BV(PD2); // data high
    else
      PORTD &= ~_BV(PD2); // data low

    PORTD |= _BV(PD3); // pin 3 high (clock)
    PORTD &= ~_BV(PD3); // pin 3 low
  }
}
