#include <Servo.h>

const byte trigPin = 8;
const byte echoPin = 9;

long duration;
int distance = 0;

Servo servo;

void setup() {
  // put your setup code here, to run once:
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(9600);

  servo.attach(10);
}

void loop() {
  // put your main code here, to run repeatedly:
  for (int i = 15; i <= 165; i++) {
    servo.write(i);
    delay(30);
    int tmp = calculateDistance();
    if (tmp < 40) {
      distance = tmp;
    }

    Serial.print(i);
    Serial.print(",");
    Serial.print(distance);
    Serial.print(".");
  }

  for (int i = 165; i >= 15; i--) {
    servo.write(i);
    delay(20);
    int tmp = calculateDistance();
    if (tmp < 40) {
      distance = tmp;
    }

    Serial.print(i);
    Serial.print(",");
    Serial.print(distance);
    Serial.print(".");
  }
}

int calculateDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  return distance;
}

