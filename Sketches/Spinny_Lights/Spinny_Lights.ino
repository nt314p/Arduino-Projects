//Pins are 12, 11, 10, and 9

int call;
float tdelay = 100;
int dividen = 6;

void setup() {
  // put your setup code here, to run once:
  pinMode(12, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(9, OUTPUT);
  
}

void loop() {
  // put your main code here, to run repeatedly:
  call = ChangePins(1, 1, tdelay);
  tdelay = analogRead(A0)/dividen;
  call = ChangePins(2, 1, tdelay);
  tdelay = analogRead(A0)/dividen;
  call = ChangePins(2, 2, tdelay);
  tdelay = analogRead(A0)/dividen;
  call = ChangePins(1, 2, tdelay);
  tdelay = analogRead(A0)/dividen;
}

int ChangePins (int Row, int Column, int onfor) {
  switch (Row) {
    case 1:
      digitalWrite(10, LOW);
      switch (Column) {
        case 1:
          digitalWrite(11, HIGH);
        break;
        case 2:
          digitalWrite(12, HIGH);
        break;
      }
    break;
    case 2:
      digitalWrite(9, LOW);
      switch (Column) {
        case 1:
          digitalWrite(11, HIGH);
        break;
        case 2:
          digitalWrite(12, HIGH);
        break;
      }
    break;
  }
  delay(onfor);
  call = Reset();
}

int Reset () {
  digitalWrite(12, LOW);
  digitalWrite(11, LOW);
  digitalWrite(10, HIGH);
  digitalWrite(9, HIGH);
}

