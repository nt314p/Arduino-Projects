#include <SoftwareSerial.h>

#define rxPin 8
#define txPin 9
#define baudRate 38400

SoftwareSerial softwareSerial (rxPin, txPin);

void setup() {
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  
  softwareSerial.begin(baudRate);
  Serial.begin(baudRate);
  Serial.println("Started!");
}

void loop() {
  while (softwareSerial.available() > 0) {
    Serial.write(softwareSerial.read());
  }
  while (Serial.available() > 0)  {
    softwareSerial.write(Serial.read());
  }
}
