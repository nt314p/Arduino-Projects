#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define WRITE_EN 13
#define EEPROM_WORDS 32768
#define ERASE_CONFIRMATION 0xBEEF

enum State : uint8_t {
  Idle,
  Address,
  Parameter
};

enum Command : uint8_t {
  None,
  Read,
  Write,
  Load,
  Dump,
  Erase
};

const char validCommands[] = { 'r', 'w', 'l', 'd', 'e' };
const Command enumCommands[] = { Command::Read, Command::Write, Command::Load, Command::Dump, Command::Erase };
uint16_t address = 0;
uint16_t parameter = 0;
uint16_t byteCount = 0;

uint8_t page[64];
uint8_t pageCount = 0;

State currentState = State::Idle;
Command currentCommand = Command::None;

// Command consists of a single identifier char followed by [PARAMETER(byte size)] in byte form
// READ: r[ADDRESS(2)] // 2 bytes
// WRITE: w[ADDRESS(2)][DATA(1)] // 3 bytes
// LOAD: l[START_ADDRESS(2)][COUNT(2)][DATA(COUNT)] // 4 + COUNT bytes
// DUMP: d[START_ADDRESS(2)][COUNT(2)] // 4 bytes
// ERASE: e[ADDRESS(2)] // 2 bytes, note: the address must equal BEEF for successful erase (accidental erase prevention)

// 259 bytes/s write
// 11.8 kilobytes/s read

inline void shiftOutFaster(uint16_t value) __attribute__((always_inline));
inline uint8_t rawReadEEPROM() __attribute__((always_inline));

void setup() {
  // set data pins direction as input
  DDRD &= B00011111;  // PD5, PD6, PD7 inputs (low), pins 5, 6, 7
  DDRB &= B11100000;  // PB0, PB1, PB2, PB3, PB4 inputs, pins 8, 9, 10, 11, 12

  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(WRITE_EN, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  digitalWrite(SHIFT_CLK, LOW);
  digitalWrite(SHIFT_LATCH, LOW);

  setAddress(0, false);  // disable output

  Serial.begin(115200);
}

void loop() {
  if (!Serial.available()) return;

  uint8_t input = Serial.read();

  if (currentState == State::Idle) {  // input is command char
    bool isValidCommand = false;
    for (uint8_t i = 0; i < sizeof(validCommands); i++) {
      if (validCommands[i] != (char)input) continue;

      currentCommand = enumCommands[i];
      isValidCommand = true;
      break;
    }

    if (!isValidCommand) {
      Serial.print(F("Invalid command: "));
      Serial.println((char)input);
      resetState();
      return;
    }

    currentState = State::Address;
  } else if (currentState == State::Address) {
    if (byteCount == 0) {
      address |= input;  // read in upper byte
      address <<= 8;
      byteCount++;
      return;
    } else if (byteCount == 1) {
      address |= input;  // read in lower byte
      byteCount++;

      if (currentCommand == Command::Read) {
        Serial.write(readEEPROM(address));
        resetState();
        return;
      } else if (currentCommand == Command::Erase) {
        if (address != ERASE_CONFIRMATION) {
          Serial.write(0);  // confirmation did not match
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
      parameter |= input;
      byteCount++;
      if (currentCommand == Command::Write) {
        writeEEPROM(address, parameter);
        Serial.write(Serial.available());
        resetState();
      }
      return;
    } else if (byteCount == 3) {
      parameter <<= 8;
      parameter |= input;
      byteCount++;

      if (currentCommand == Command::Dump) {
        for (unsigned int i = 0; i < parameter; i++) {
          Serial.write(readEEPROM(address + i));
        }
        resetState();
      }

      return;
    }

    if (currentCommand != Command::Load) return;

    uint8_t pageAddress = address & B00111111;

    page[pageAddress] = input;
    pageCount++;
    byteCount++;
    Serial.write(Serial.available());

    // reached end of page OR end of load, write page
    if (pageAddress == 63 || byteCount >= parameter + 4) {
      writePageEEPROM((address + 1) - pageCount, pageCount);
      pageCount = 0;
    }

    if (byteCount >= parameter + 4) {
      resetState();
      return;
    }

    address++;
  }
}

void resetState() {
  byteCount = 0;
  address = 0;
  parameter = 0;
  pageCount = 0;
  currentState = State::Idle;
  currentCommand = Command::None;
}

void eraseEEPROM() {
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0x80);
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0x10);
  delay(20);  // wait for chip erase
  disableWriteProtection();
}

void disableWriteProtection() {
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0x80);
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0x20);
  delay(1);
}

// Read byte at specified address
uint8_t readEEPROM(uint16_t address) {
  // set data pins direction as input
  DDRD &= B00011111;  // PD5, PD6, PD7 inputs (low), pins 5, 6, 7
  DDRB &= B11100000;  // PB0, PB1, PB2, PB3, PB4 inputs, pins 8, 9, 10, 11, 12

  setAddress(address, /*outputEnable*/ true);

  asm volatile(  // delay 4 clock cycles (250 ns)
    "nop\n\t"
    "nop\n\t"
    "nop\n\t"
    "nop\n\t");

  // MSB is pin 12, LSB is pin 5
  //                   12---8                    7-5
  //return ((PINB & B00011111) << 3) + ((PIND & B11100000) >> 5);
  return (PINB << 3) | (PIND >> 5);
}

uint8_t rawReadEEPROM() {
  return (PINB << 3) | (PIND >> 5);
}

void writeEEPROM(uint16_t address, uint8_t data) {
  setAddress(address, false);

  // set data pins direction as ouput
  DDRD |= B11100000;
  DDRB |= B00011111;

  // clear data bits, and then set
  PORTD = (PORTD & B00011111) | (data << 5);
  PORTB = (PORTB & B11100000) | (data >> 3);

  // toggle write enable
  PORTB &= ~_BV(PB5);
  // delay 2 clock cycle (125 ns)
  asm volatile("nop\n\tnop\n\t");
  PORTB |= _BV(PB5);

  uint8_t r = readEEPROM(address);
  while (r != data) {
    r = rawReadEEPROM();
  }
}

void writePageEEPROM(uint16_t startAddress, uint8_t count) {
  uint8_t offset = B00111111 & startAddress; // lower 6 bits

  setAddress(startAddress, false);

  // set data pins direction as ouput
  DDRD |= B11100000;
  DDRB |= B00011111;

  uint8_t i = 0;

  while (true) {
    // clear data bits, and then set
    PORTD = (PORTD & B00011111) | (page[offset + i] << 5);
    PORTB = (PORTB & B11100000) | (page[offset + i] >> 3);

    // toggle write enable
    PORTB &= ~_BV(PB5);
    // delay 2 clock cycle (125 ns)
    asm volatile("nop\n\tnop\n\t");
    PORTB |= _BV(PB5);

    i++;

    if (i >= count) break;
    setAddress(startAddress + i, false);
  }

  uint8_t lastData = page[offset + count - 1];
  uint8_t r = readEEPROM(startAddress + count - 1);

  while (r != lastData) { // wait until the last byte is read back correctly, indicates success
    r = rawReadEEPROM(); // reads EEPROM without setting address, faster
  }
}

void setAddress(uint16_t address, bool outputEnable) {
  shiftOutFaster(address | (outputEnable ? 0x00 : 0x8000));
}

// takes ~15.5 us
void shiftOutFaster(uint16_t value) {
  for (uint8_t i = 0; i < 16; ++i) {

    // check MSB
    if (value & (1 << 15)) {
      PORTD |= _BV(PD2);
    } else {
      PORTD &= ~_BV(PD2);
    }

    // pin 3 toggle clock
    PORTD |= _BV(PD3);
    PORTD &= ~_BV(PD3);

    value <<= 1;
  }

  // pin 4 toggle latch
  PORTD |= _BV(PD4);
  PORTD &= ~_BV(PD4);
}