
// shift register pins (out)
const int clockPin = 9; // Pin 11 of register: pin for clocking i/o data (used for both registers)
const int latchPin = 7; // Pin 12 of register: low before writing data, high to display data
const int dataOutPin = 8; // Pin 14 of register: pin for serialOut (data write)


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // shift register pins
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataOutPin, OUTPUT);
  
  delay(100);
  writeRegister(240);
  writeRegister(251);

}

void loop() {
  // put your main code here, to run repeatedly:
  
}

void writeRegister(byte dataIn) { //LEAST SIGNIFICANT
  // latch to low so bits don't show (hey that rhymes)
  digitalWrite(latchPin, LOW);

  // shift out the bits:
  shiftOut(dataOutPin, clockPin, LSBFIRST, dataIn);

  // latch to high so show bits
  digitalWrite(latchPin, HIGH);
}
