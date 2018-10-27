#include <Servo.h>


Servo buttonServo;

const int tripPin = 12;
const int speakerPin = 11;
const int servoPin = 5;
bool tripped = false;
bool triggered = false;

void setup() {
  // put your setup code here, to run once:
  buttonServo.attach(5);
  buttonServo.write(100);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (digitalRead(tripPin)&&!triggered) {
    tripped = true;
    triggered = true; //TTTRRRRRIGGERED BRRROOOOOO
  }
  
  if (tripped) {
    for (int i = 0; i < 3; i++) {
      FlashDaRacoon();
    }
    tripped = false;
  }
}

void FlashDaRacoon() {
  buttonServo.write(40);
  delay(1000);
  buttonServo.write(100);
  delay(5000);
}

