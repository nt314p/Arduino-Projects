int latchPin = 8;
int clockPin = 12;
int dataPin = 11;


void setup() {
  // put your setup code here, to run once:
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  for (int i = 0; i< 64; i++){
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, i);
  digitalWrite(latchPin, HIGH);
  delay(200);
  }
}
