volatile float a = 0;
volatile float b = 123.532352;
volatile float c = 19.429;

volatile uint16_t d = 0;
volatile uint16_t e = 41412;
volatile uint16_t f = 124;

/*
Starting...
2.21
Integer addition: 1.26
Integer multiplication: 1.76
Integer division: 13.83
Integer shift: 1.83
Floating addition: 8.55
Floating multiplication: 10.12
Floating division: 32.19
*/

void setup() {
  Serial.begin(38400);
  Serial.println("Starting...");

  unsigned long startUs = micros();

  for (volatile long i = 0; i < 10000; i++) {}

  unsigned long diff = micros() - startUs;
  Serial.println(diff / 10000.0);



  startUs = micros();

  for (long i = 0; i < 100000; i++) {
    d = e + f;
  }

  diff = micros() - startUs;
  Serial.print("Integer addition: ");
  Serial.println(diff / 100000.0);

  startUs = micros();

  for (long i = 0; i < 100000; i++) {
    d = e * f;
  }

  diff = micros() - startUs;
  Serial.print("Integer multiplication: ");
  Serial.println(diff / 100000.0);

  startUs = micros();

  for (long i = 0; i < 100000; i++) {
    d = e / f;
  }

  diff = micros() - startUs;
  Serial.print("Integer division: ");
  Serial.println(diff / 100000.0);

  startUs = micros();

  for (long i = 0; i < 100000; i++) {
    d = e >> 3;
  }

  diff = micros() - startUs;
  Serial.print("Integer shift: ");
  Serial.println(diff / 100000.0);

  startUs = micros();

  for (long i = 0; i < 100000; i++) {
    a = b + c;
  }

  diff = micros() - startUs;
  Serial.print("Floating addition: ");
  Serial.println(diff / 100000.0);

  startUs = micros();

  for (long i = 0; i < 100000; i++) {
    a = b * c;
  }

  diff = micros() - startUs;
  Serial.print("Floating multiplication: ");
  Serial.println(diff / 100000.0);

  startUs = micros();

  for (long i = 0; i < 100000; i++) {
    a = b / c;
  }

  diff = micros() - startUs;
  Serial.print("Floating division: ");
  Serial.println(diff / 100000.0);
}

void loop() {
  delay(500);
}
