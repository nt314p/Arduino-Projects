#include <Servo.h>
#include <math.h>

const byte trigPin = 10;
const byte echoPin = 11;

Servo rightShoulder;
Servo rightElbow;
Servo leftShoulder;
Servo leftElbow;

const int RSpin = 5;
const int REpin = 3;
const int LSpin = 9;
const int LEpin = 6;

const int movedelay = 200;

int distance;
float duration;
int noiseCount = 0;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);      //distance sensor pins

  pinMode(RSpin, OUTPUT);
  pinMode(REpin, OUTPUT);       //shoulder/knee pins
  pinMode(LSpin, OUTPUT);
  pinMode(LEpin, OUTPUT);

  Serial.begin(9600);

  rightShoulder.attach(RSpin);
  rightElbow.attach(REpin);
  leftShoulder.attach(LSpin);
  leftElbow.attach(LEpin);
  
  initialize();
  delay(1000);

  
}

void l() {
  rightShoulder.write(90);
  distance = calculateDistance();
  delay(movedelay);
  rightElbow.write(45);
  distance = calculateDistance();
  delay(movedelay);
  rightShoulder.write(15);
  distance = calculateDistance();
  delay(movedelay);
  rightElbow.write(100);
  distance = calculateDistance();
  delay(movedelay);
}

void r() {
  leftShoulder.write(25);
  distance = calculateDistance();
  delay(movedelay);
  leftElbow.write(70);
  distance = calculateDistance();
  delay(movedelay);
  leftShoulder.write(100);
  distance = calculateDistance();
  delay(movedelay);
  leftElbow.write(15);
  distance = calculateDistance();
  delay(movedelay);
}

void walk() {
  r();
  l();
}

void initialize() {
  rightShoulder.write(100);
  rightElbow.write(100);
  leftShoulder.write(15);
  leftElbow.write(15);
  delay(1000);
}

void loop() {
  autonomous();
  //Serial.println(calculateDistance());
  //delay(80);
}

void autonomous() {
  r();
  
  if (calculateDistance() < 10) {
    r();
  } else {
    l();
  }

}

int calculateDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  int tmp = duration * 0.034 / 2;
  if (!tmp == 0) {
    int diff = abs(distance - tmp);
    if (diff < 15 || noiseCount > 5) {
      noiseCount = 0;
      distance = tmp;
    } else {
      noiseCount++;
    }
  }
  Serial.println(distance);
  return distance;
}
