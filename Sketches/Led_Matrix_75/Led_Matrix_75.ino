#include "pitches.h"
#include "Timer.h"
#include "PixelMatrix.h"
#include "RGBLed.h"

const byte width = 7;
const byte height = 5;

// pin numbers for both width and height are taken care of using shift registers
const boolean reverseWidth = false; // Q0 should be the leftmost led, if not, set to true
const boolean reverseHeight = false; // same usage, but for the height

volatile int updateCounter = 0; // the counter that drives the LED matrix display

/* Dual Shift Register diagram:
    REG ONE               REG TWO
     0                    8
  -|-|-|-|-|-|-|-|-    -|-|-|-|-|-|-|-|-
  >               |    >               |
  -|-|-|-|-|-|-|-|-    -|-|-|-|-|-|-|-|-
   1 2 3 4 5 6 7        9   11  13  15
                          10  12  14
  0, 1, 2, 3, 4 -> HIGH (for height)
  5, 6, 7, 8, 9, 10, 11 -> LOW (for width)
  12, 13, 14 -> R, G, B of RGB led
  15 -> UNUSED
*/

// shift register pins (out)
const byte clockPin = 9; // Pin 11 of register: pin for clocking i/o data (used for both registers)
const byte latchPin = 7; // Pin 12 of register: low before writing data, high to display data
const byte dataOutPin = 8; // Pin 14 of register: pin for serialOut (data write)
const byte enablePin = 6; // Pin 13 of register: LOW for ON, HIGH for OFF

// shift register pins (in)
const byte clockInhPin = 12; // Pin 15 of register: pin low to clock in data (high to fill registers)
const byte ploadPin = 11; // Pin 2 of register: pin low to load internal registers with input values (high to clock in data)
const byte dataInPin = 10; // Pin 9 of the register: the input pin that the data shifts out to

const byte speakerPin = 13; // speaker pin

const byte updatePeriod = 10; // in microseconds
const byte GSdutyCycle[][2] = { {0, 0}, {1, 2}, {2, 4}, {3, 8}}; // {greyscale value, updatePeriod*scaler}

// the pixel representation of the screen
byte pixels[35] = {
  0, 3, 0, 3, 0, 3, 0,
  0, 3, 0, 3, 0, 3, 0,
  0, 3, 0, 3, 0, 3, 0,
  0, 3, 0, 3, 0, 3, 0,
  0, 3, 0, 3, 0, 3, 0
};


// RGBLed rgbLed = new RGBLed(255, 255, 0);


// forward declaration of timer functions
void meeper();
void getBatt();
void calcStreamers();

Timer meep(*meeper, 2000, false);
Timer battCheck(*getBatt, 4000, true);
Timer strUpdate(*calcStreamers, 200, false);

Timer timers[] = {meep, battCheck, strUpdate}; // array of all Timer objects

// battery monitor pin (raw)
const byte battPin = 14; // A0

// low battery pin (cooked) LOOK INTO: light goes on when LB is connected to A1
const byte lowBattPin = 15; //A1

// timer shared between all apps
unsigned long lastMillisApp = 0;
int ivalApp = 200;

int letterplace = 0;

// debug input a1:
int monPin = A1;

// streamer variables
int strmr = 2; // position
int len = 0; // how long the streamer goes in a single direction
int dir = 0; // which direction in the streamer going in? -1, 0, 1

// menu
byte menuIcons[35] = {
  0, 0, 3, 3, 3, 0, 0,
  0, 3, 0, 0, 0, 3, 0,
  0, 3, 0, 3, 0, 3, 0,
  0, 3, 0, 0, 0, 3, 0,
  0, 0, 3, 3, 3, 0, 0
};

boolean menuShifting = false;
int currMenu = 0; // if negative, an app is running

/*
   0 timer
   1 ?
*/

// buttons
boolean upPress = false;
boolean downPress = false;
boolean leftPress = false;
boolean rightPress = false;
boolean alphaPress = false;
boolean betaPress = false;

// the alphabet: each row is two letters
const byte alphabet[20] = {
  105, 159, 158, 158, 158,
  120, 136, 126, 153, 158,
  248, 232, 255, 142, 136,
  120, 185, 121, 159, 153
};

void setup() {
  Serial.begin(9600);

  // shift register pins
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataOutPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(clockInhPin, OUTPUT);
  pinMode(ploadPin, OUTPUT);
  digitalWrite(enablePin, HIGH);
  digitalWrite(ploadPin, LOW);
  digitalWrite(clockInhPin, HIGH);

  pinMode(speakerPin, OUTPUT); // setting up the speaker pin
  digitalWrite(speakerPin, LOW);

  // battery sensing
  pinMode(battPin, INPUT);
  pinMode(lowBattPin, INPUT);

  // setting up interupt timer 0 to run every 1 ms
  OCR0A = 0xAF;
  TIMSK0 |= (1 << OCIE0A); // enabling compare interrupt

  // 20,000 HZ for 50 microseconds

  byte compareMatchRegister = (16000000) / (1000000 / updatePeriod) - 1; // (must be <65536)
  TCCR1A = 0; // clearing timer 1 registers
  TCCR1B = 0;
  OCR1A = compareMatchRegister;
  TCCR1B |= (1 << WGM12); // turn onCTC mode
  TCCR1B |= (1 << CS10); // setting CS10 bit for 1 prescaling (none)
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt

  //displayPixels();
}

void loop() {

  // printInput();
  // readBatt();

}

void getBatt () { // careful, messes with interrupts
  float val = analogRead(battPin);
  val *= 5;
  val /= 1024;
  noInterrupts();
  Serial.print("Battery Voltage at: ");
  Serial.print(val);
  Serial.println("V");
  interrupts();
}

void meeper() {
  Serial.println("MEEEEEP!");
}

void printInput() {
  updateButtonStates(readRegister());
  Serial.print("UP:");
  Serial.println(upPress);
  Serial.print("LEFT:");
  Serial.println(leftPress);
  Serial.print("DOWN:");
  Serial.println(downPress);
  Serial.print("RIGHT:");
  Serial.println(rightPress);
  Serial.print("ALPHA:");
  Serial.println(alphaPress);
  Serial.print("BETA:");
  Serial.println(betaPress);
}

void calcStreamers () {

  if (strmr == 0) {
    dir = random(0, 2); // setting random direction
    len = random(0, 3); // setting new random length

  } else if (strmr == 4) {
    dir = random(-1, 1);
    len = random(0, 3); // setting new random length

  } else if (len == 0) {
    dir = random(-1, 2);
    len = random(0, 3); // setting new random length

  } else {
    len--;
  }

  strmr += dir; // adding direction

  switch (strmr) { // shifting out single pixels
    case 0:
      shiftLeft(3, 2, 1, 0, 0);
      break;
    case 1:
      shiftLeft(2, 3, 2, 1, 0);
      break;
    case 2:
      shiftLeft(1, 2, 3, 2, 1);
      break;
    case 3:
      shiftLeft(0, 1, 2, 3, 2);
      break;
    case 4:
      shiftLeft(0, 0, 1, 2, 3);
      break;
  }
}

//void writeRGB

void displayPixels() {
  // resetRegisters();
  int i = 0;
  int j = 0;

  byte wData = 1; // width data (7 bits) enable LOW   0b0111 1111
  byte hData = 1; // height data (5 bits) enable HIGH 0b0001 1111

  if (reverseHeight) {
    hData = 0b00010000; // 0001 0000
  } else {
    hData = 0b00000001; // 0000 0001
  }

  for (j = 0; j < height; j++) {
    if (reverseWidth) { // if we reverse, we want to start large
      wData = 0b01000000; // 0100 0000
    } else {
      wData = 0b00000001; // resetting data for next loop
    }

    for (i = 0; i < width; i++) {
      // seeing if the corresponding pixel was t or f
      if (pixels[i + j * width] > 0) {
        // resetRegisters();
        writeMatrixRegisters(wData, hData);
      }

      // GRAYSCALE!
      switch (pixels[i + j * width]) {
        case 0:
          break;
        case 1:
          delayMicroseconds(50);
          break;
        case 2:
          delayMicroseconds(100);
          break;
        case 3:
          delayMicroseconds(200);
          break;
      }

      if (reverseWidth) { // SHIFTING THE BIT
        wData = wData >> 1; // if we reverse, we want to count down (shift bit right)
      } else {
        wData = wData << 1; // shifting the "1" over one place left (binary rules)
      }
    }

    if (reverseHeight) { // SHIFTING THE BIT
      hData = hData >> 1; // if we reverse, we want to count down (shift bit right)
    } else {
      hData = hData << 1; // shifting the "1" over one place left (binary rules)
    }
  }
  // displayPixels();
}

void shiftLeft(byte a, byte b, byte c, byte d, byte e) {
  int i = 0;
  int j = 0;
  for (j = 0; j < height; j++) {
    for (i = 0; i < width - 1; i++) { // -1 since we don't want to shift the last bit into non existance
      pixels[i + j * width] = pixels[i + j * width + 1]; // each pixel is set to the one on the left
    }
    switch (j) { // setting last pixel in row based on j value (height)
      case 0:
        pixels[6] = a;
        break;
      case 1:
        pixels[13] = b;
        break;
      case 2:
        pixels[20] = c;
        break;
      case 3:
        pixels[27] = d;
        break;
      case 4:
        pixels[34] = e;
        break;
    }
  }
}

void writeMatrixRegisters(byte w, byte h) { // w should be non inverted
  byte wData = ~w; // inverting w because ENABLE LOW (in the form 0b0xxx xxxx)
  byte hData = h; // in the form 0b000x xxxx
  byte wUpper = (wData & 0b01110000) >> 4; // 0111 0000, masking, then shifting right 4 places
  byte wLower = wData & 0b00001111;  // 0000 1111
  byte regTwo = wLower << 4; // shift bits over 4 left
  byte regOne = hData << 3; // shift bits over 3 left
  regOne = regOne + wUpper;
  // regOne: 0bHHHHHWWW // height, width
  // regTwo: 0bWWWWRGB0 // width, RGB data (not yet implemented)

  digitalWrite(enablePin, HIGH); // disable output
  writeRegister(regTwo);
  writeRegister(regOne);
  digitalWrite(enablePin, LOW); // enable
}

void resetRegisters() { // i say dont use it
  digitalWrite(enablePin, HIGH); // disable output
  writeRegister(240); // second register
  writeRegister(7); // first register
  // 0000 0111   1111 1111

  digitalWrite(enablePin, LOW); // enable

}

// takes in a byte of data and stores it in the button booleans
void updateButtonStates(byte state) { // TODO: pack the bits into one byte
  upPress = (state >> 0) & 1; // shifting bits over to read them individually
  leftPress = (state >> 1) & 1;
  downPress = (state >> 2) & 1;
  rightPress = (state >> 3) & 1;
  alphaPress = (state >> 6) & 1;
  betaPress = (state >> 5) & 1;
}

byte readRegister() {
  digitalWrite(enablePin, HIGH); // clock is shared, disabling out registers

  byte incoming;
  // pulsing parallel load pin to load internal registers with current parallel input
  // clock inhibit must also be pulsed
  digitalWrite(ploadPin, LOW);
  digitalWrite(clockInhPin, HIGH);
  delayMicroseconds(1);
  digitalWrite(ploadPin, HIGH);
  digitalWrite(clockInhPin, LOW);

  digitalWrite(clockPin, LOW); // writing clock low to prepare for shifting in

  for (int i = 0; i < 8; i++) {
    incoming *= 2; // multiplying by 2 to shift bits to the left;
    incoming = incoming | digitalRead(dataInPin); // "or" ing the incoming bit (basically adding it to the end)
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(1);
    digitalWrite(clockPin, LOW);
  }

  //digitalWrite(enablePin, LOW); // re-enabling out registers

  return incoming;
}

void writeRegister(byte dataIn) {

  // latch to low so bits don't show (hey that rhymes)
  digitalWrite(latchPin, LOW);
  // shift out the bits:
  shiftOut(dataOutPin, clockPin, LSBFIRST, dataIn);

  // latch to high so show bits
  digitalWrite(latchPin, HIGH);
}

ISR(TIMER0_COMPA_vect) { // Interrupt timer runs every 1 ms

  unsigned long currentMillis = millis();

  // looping through array of timers to update them all
  for (int i = 0; i < sizeof(timers) / sizeof(Timer); i++) {
    timers[i].refresh(currentMillis);
  }

  if (currentMillis % 1000 == 0) {
    Serial.print(currentMillis / 1000);
    Serial.println(" seconds since startup");
  }
}

ISR(TIMER1_COMPA_vect) {
  // runs every updatePeriod microseconds
  updateCounter %= (width * height * 4 * 32); // rollover reset

  //noInterrupts();
  //interrupts();

  byte pixelState = (updateCounter % (width * height * 4)) / 4; // 0 - 34, the pixel # are we on
  byte greyScaleState = updateCounter % 4; // 0 - 3, every 4 cycles
  byte rgbState = updateCounter % 32; // 0 - 31, the RGB led states PWM 32
  byte greyScaleValue = pixels[pixelState]; // getting the greyscale value (0 - 3)

//  Serial.println(updateCounter);
//  Serial.print(greyScaleState);
//  Serial.print(", ");
//  Serial.println(greyScaleValue);

  if (greyScaleValue != 0 && greyScaleValue >= greyScaleState) {
    byte wData = 1 << (pixelState % width); // width data (7 bits) enable LOW   0b0111 1111
    byte hData = 1 << (pixelState / width); // height data (5 bits) enable HIGH 0b0001 1111
    writeMatrixRegisters(wData, hData);
  } else {
    resetRegisters();
  }

  updateCounter++;
}



