#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13
#define EEPROM_WORDS 32768
#define ERASE_CONFIRMATION 0xBEEF

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
int address = 0;
unsigned int parameter = 0;
bool isUpperNibble = false;

State currentState = State::Idle;
Command currentCommand = Command::None;

// Commands consist of a single char, followed by parameters (in hex) separated by commas. A semicolon ';' ends each command.
// READ: r[ADDRESS];         // 4 = 4 bytes
// WRITE: w[ADDRESS],[DATA]; // 4 + 1 + 2 = 7 bytes
// LOAD: l[START_ADDRESS],[DATA]; // cannot buffer parameters
// DUMP: d[START_ADDRESS],[#BYTES (hex)]; //  4 + 1 + 4 = 9 bytes
// PRINT: p[START_ADDRESS],[#BYTES (hex)]; // 4 + 1 + 4 = 9 bytes
// ERASE: e[ADDRESS]; // note: the address must equal BEEF for successful erase (accidental erase prevention)
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

  Serial.begin(115200);
  Serial.println("Initialized");

  //disableWriteProtection();
}

void loop() {
  if (!Serial.available()) return;

  byte input = Serial.read();

  if (currentState == State::Idle) {  // input is command char
    bool isValidCommand = false;
    for (int i = 0; i < sizeof(validCommands); i++) {
      if (validCommands[i] != input) continue;

      currentCommand = enumCommands[i];
      isValidCommand = true;
      break;
    }

    if (!isValidCommand) {
      Serial.println("Invalid command!");
      resetState();
      return;
    }

    currentState = State::Address;
  } else if (currentState == State::Address) {  // input is char of hex address
    byte b = hexToByte(input);                  // attempt to parse address hex char

    if (b != 255) {
      address <<= 4;
      address += b;
      return;
    }

    if (input == ',') {
      if (currentCommand != Command::Read || currentCommand != Command::Erase) {
        currentState = State::Parameter;  // comma signals transition to data/parameter parsing (except for read and erase)
        return;
      }

      Serial.println("Unexpected character ','");
      resetState();
      return;
    } else if (input == ';') {  // for read and erase commands, semicolon marks the end of command
      switch (currentCommand) {
        case Command::Read:
          Serial.println(readEEPROM(address), HEX);
          Serial.println("Read successful");
          break;
        case Command::Erase:
          if (address != ERASE_CONFIRMATION) {
            Serial.println("Erase confirmation was incorrect");
          } else {
            eraseEEPROM();
            Serial.println("Erase successful");
          }
          break;
        default:
          Serial.println("Unexpected character ';'");
          break;
      }

      resetState();
      return;
    }

    Serial.println("Invalid hex character");
    resetState();
    return;
  } else if (currentState == State::Parameter) {
    byte b = hexToByte(input);

    if (b == 255) {
      if (input != ';') {
        Serial.println("Invalid hex character");
        resetState();
        return;
      }

      switch (currentCommand) {
        case Command::Write:
          forceWriteEEPROM(address, parameter);
          Serial.write(Serial.available());
          Serial.println("Write successful");
          break;
        case Command::Load:
          Serial.println("Load successful");
          break;
        case Command::Dump:
          for (int i = 0; i < parameter; i++) {
            Serial.print(readEEPROM(address + i), HEX);
          }
          Serial.println("\nDump successful");
          break;
        case Command::Print:
          commandPrint(address, parameter);
          Serial.println("Print successful");
          break;
      }

      resetState();
      return;
    }

    parameter <<= 4;
    parameter += b;

    if (currentCommand != Command::Load) return;

    if (!isUpperNibble) {  // write two nibbles (one byte) at a type
      isUpperNibble = true;
    } else {
      smartWriteEEPROM(address, parameter);
      address++;
      parameter = 0;
      isUpperNibble = false;
    }
  }
}

void resetState() {
  address = 0;
  parameter = 0;
  currentState = State::Idle;
  currentCommand = Command::None;
}

void commandPrint(int address, unsigned int numBytes) {
  Serial.println(F("       0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F"));

  char addrLabel[4];  // buffer for address label
  char byteLabel[2];  // buffer for byte to be printed

  for (unsigned int i = 0; i < numBytes; i += 16) {                    // iterate through the required number of bytes to print
    byte bytesInRow = ((numBytes - i) / 16 == 0) ? numBytes - i : 16;  // normally 16, but last row might not have full 16
    int addr = address + i;                                            // address of the row

    sprintf(addrLabel, "%04X ", addr);
    Serial.print(addrLabel);
    for (int j = 0; j < bytesInRow; j++) {
      sprintf(byteLabel, " %02X", readEEPROM(addr + j));
      Serial.print(byteLabel);
      if (j == 7)
        Serial.print(" ");  // extra padding
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
  delay(20);  // wait for chip erase
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
byte readEEPROM(int address) {
  // set data pins direction as input
  DDRD &= B00011111;  // PD5, PD6, PD7 inputs (low), pins 5, 6, 7
  DDRB &= B11100000;  // PB0, PB1, PB2, PB3, PB4 inputs, pins 8, 9, 10, 11, 12

  setAddress(address, /*outputEnable*/ true);

  // MSB is pin 12, LSB is pin 5
  //                 12---8                    7-5
  return ((PINB & B00011111) << 3) + ((PIND & B11100000) >> 5);
}

// Reads a byte from the currently set address (needs to have output enable set)
// Will not set output enable or pin directions
byte readEEPROMNoAddress() {  // TODO: replace all bytes with uint8_t?
  return ((PINB & B00011111) << 3) + ((PIND & B11100000) >> 5);
}

// Writes a value and then waits until the data read back is valid
void smartWriteEEPROM(int address, byte data) {
  Serial.print("Writing ");
  Serial.println(data);

  if (readEEPROM(address) == data) return;

  Serial.println("Write necessary");

  forceWriteEEPROM(address, data);
  byte r = readEEPROM(address);
  Serial.println(r);

  while (r != data) {
    r = readEEPROM(address);
    Serial.println(r);
  }
}

// Force write a byte to EEPROM (ignores write optimization where it checks for existing value)
void forceWriteEEPROM(int address, byte data) {
  setAddress(address, false);

  // set data pins direction as ouput
  DDRD |= B11100000;
  DDRB |= B00011111;

  PORTD &= B00011111;  // clear bits
  PORTB &= B11100000;
  PORTD |= data << 5;  // write bits 2,1,0
  PORTB |= data >> 3;  // write bits 7,6,5,4,3

  PORTB &= ~_BV(PB5);  // low
  delay(1);
  PORTB |= _BV(PB5);   // high
  delay(1);
}

// Write a byte to the EEPROM at the specified address
void writeEEPROM(int address, byte data) {
  if (readEEPROM(address) == data) return;
  forceWriteEEPROM(address, data);
}

void shiftOutFaster(uint8_t val) {
  for (byte i = 0; i < 8; ++i) {
    if (!!(val & (1 << (7 - i))))
      PORTD |= _BV(PD2);  // data high
    else
      PORTD &= ~_BV(PD2);  // data low

    PORTD |= _BV(PD3);   // pin 3 high (clock)
    PORTD &= ~_BV(PD3);  // pin 3 low
  }
}

// Output the address bits and outputEnable signal using shift registers.
void setAddress(int address, bool outputEnable) {
  shiftOutFaster((address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOutFaster(address);

  // pin 4 latch toggle
  PORTD |= _BV(PD4);   // high
  PORTD &= ~_BV(PD4);  // low
}

byte hexToByte(char hex) {
  if (hex >= 97) {  // lowercase a-f
    hex -= 32;      // capitalize
  }
  if ('0' <= hex && hex <= '9') {  // 0 - 9
    return hex - '0';
  }
  if (65 <= hex && hex <= 70) {  // A - F
    return hex - 55;
  }
  return 255;
}
