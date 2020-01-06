// Most code taken from https://github.com/beneater/eeprom-programmer/blob/master/eeprom-programmer/eeprom-programmer.ino
// Credit goes to Ben Eater

#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13
#define EEPROM_WORDS 32768
// const char[] hex = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

char cmdMode = '-';
int parameter = 0;
boolean onData = false;
int wData = 0;




// Output the address bits and outputEnable signal using shift registers.
void setAddress(int address, bool outputEnable) {
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}


// Read a byte from the EEPROM at the specified address.
byte readEEPROM(int address) {

  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    pinMode(pin, INPUT);
  }

  setAddress(address, /*outputEnable*/ true);

  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin -= 1) {
    data = (data << 1) + digitalRead(pin);
  }

  return data;
}

// Write a byte to the EEPROM at the specified address.
void writeEEPROM(int address, byte data) {

  setAddress(address, /*outputEnable*/ false);

  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    pinMode(pin, OUTPUT);
  }

  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }

  digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(WRITE_EN, HIGH);
}





/*

   Read the contents of the EEPROM and print them to the serial monitor.

*/

void printContents() {

  for (int base = 0; base <= 255; base += 16) {

    byte data[16];

    for (int offset = 0; offset <= 15; offset += 1) {
      data[offset] = readEEPROM(base + offset);
    }

    char buf[80];

    sprintf(buf, "%03x:  %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",

            base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],

            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    Serial.println(buf);
  }
}

/* SOFTWARE WRITE PROTECTION DISABLE
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0x80);
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0x20);
*/



// 4-bit hex decoder for common anode 7-segment display

byte data[] = { 0x81, 0xcf, 0x92, 0x86, 0xcc, 0xa4, 0xa0, 0x8f, 0x80, 0x84, 0x88, 0xe0, 0xb1, 0xc2, 0xb0, 0xb8 };


void setup() {

  // setting up pins
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);

  Serial.begin(57600);



  /*
    // Erase entire EEPROM

    Serial.print("Erasing EEPROM");

    for (int address = 0; address <= 2047; address += 1) {

      writeEEPROM(address, 0b10101010);



      if (address % 64 == 0) {

        Serial.print(".");

      }

    }

    Serial.println(" done");
  */




  // Program data bytes

  //  Serial.print("Programming EEPROM");
  //
  //  for (int address = 0; address < sizeof(data); address += 1) {
  //
  //    writeEEPROM(address, data[address]);
  //
  //
  //
  //    if (address % 64 == 0) {
  //
  //      Serial.print(".");
  //
  //    }
  //
  //  }

}


void loop() {

  //  if (Serial.find("\n")) {
  //    int bLen = Serial.available();
  //    char buf[bLen];
  //
  //    Serial.readBytesUntil('\n', buf, bLen);
  //    Serial.print("INPUT: ");
  //    for (int i = 0; i < bLen; ++i) {
  //      Serial.print(buf[i]);
  //    }
  //    Serial.print("\n");
  //    Serial.println(hexToInt(buf));
  //  }

  if (cmdMode == '-') {
    if (Serial.available() > 0) {
      char input = Serial.read();

      if (input == 'r') {
        cmdMode = 'r';
        Serial.println("r mode");
      }
      if (input == 'w') {
        cmdMode = 'w';
        Serial.println("w mode");
      }
    }
  } else if (cmdMode == 'r') {
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input != ';') {
        parameter = parameter << 4;
        parameter = parameter | hexToInt(input);
      } else {
        Serial.print("Read from ");
        Serial.print(parameter, DEC);
        Serial.print(" is ");
        Serial.println(readEEPROM(parameter));
        cmdMode = '-';
        parameter = 0;
      }
    }
  } else if (cmdMode == 'w') {
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == ';') {
        Serial.print("Wrote: ");
        Serial.print(wData, DEC);
        Serial.print(" to ");
        Serial.println(parameter, DEC);
        writeEEPROM(parameter, wData);
        cmdMode = '-';
        parameter = 0;
        wData = 0;
        onData = false;
      } else if (input == ',') {
        onData = true;
      } else if (!onData) {
        parameter = parameter << 4;
        parameter = parameter | hexToInt(input);
      } else {
        wData = wData << 4;
        wData = wData | hexToInt(input);
      }
    }
  }
}

int hexToInt(char hex) {
  char str[] = {hex};
  char *ptr;
  int ret = strtoul(str, &ptr, 16);
  return ret;
}


/*
  const byte numChars = 32;
  char receivedChars[numChars];

  boolean newData = false;


  void loop() {
    recvWithStartEndMarkers();
    showNewData();
  }

  void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

  // if (Serial.available() > 0) {
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
  }

  void showNewData() {
    if (newData == true) {
        Serial.print("This just in ... ");
        Serial.println(receivedChars);
        newData = false;
    }
  }*/
