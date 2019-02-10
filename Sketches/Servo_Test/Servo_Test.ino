#include <Servo.h>

Servo servo;
int servoPin = 6;
int servoSpeed = 2;
int pos = 0;

int analogPin = A0;
float val = 0;

void setup() {
  // put your setup code here, to run once:
  servo.attach(servoPin);
  servo.write(90);

  analogReference(INTERNAL);

  Serial.begin(9600);
}

void loop() {

  val = analogRead(analogPin);     // read the input pin
  Serial.println(val);
  delay(100);
  //
  //  servo.write(0);
  //  delay(1000);
  //  servo.write(180);
  //  delay(1000);


  //  for (pos = 0; pos <= 180; pos += servoSpeed) { // goes from 0 degrees to 180 degrees
  //    // in steps of 1 degree
  //    servo.write(pos);              // tell servo to go to position in variable 'pos'
  //    delay(10);                       // waits 15ms for the servo to reach the position
  //  }
  //  for (pos = 180; pos >= 0; pos -= servoSpeed) { // goes from 180 degrees to 0 degrees
  //    servo.write(pos);              // tell servo to go to position in variable 'pos'
  //    delay(10);                       // waits 15ms for the servo to reach the position
  //  }
}
