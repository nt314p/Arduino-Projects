#include <MicroView.h>

double dx = 0.7;
double dy = 0.2;
double x = 20;
double y = 20;
double gravity = 0.2;
int potVal = 0;

void setup() {
  // put your setup code here, to run once:
  uView.begin();              // start MicroView
  uView.clear(PAGE);          // clear page

  pinMode(6, OUTPUT);
}

void loop() {

  digitalWrite(6, HIGH);
  delay(1000);
  digitalWrite(6, LOW);
  delay(1000);
  // put your main code here, to run repeatedly:
  /*
  potVal = analogRead(A1);
  uView.print(potVal);

  if (x + 6 > 64 || x - 6 < 0) {
    dx *= -1;
  }
  if (y + 6 > 48 || y - 6 < 0) {
    dy *= -1;
  }

  //Gravity
  if (48 > y + 6 ) {
    dy = -1 * (dy * -1.00002 - gravity);
  }

  if (y + 6 > 48 && dy > 0) {
    dy *= -1;
  }

  x += dx;
  y += dy;
  uView.clear(PAGE);
  uView.circle(x, y, 6);
  uView.display();    // display current page buffer
  delay(10);
  */
}



