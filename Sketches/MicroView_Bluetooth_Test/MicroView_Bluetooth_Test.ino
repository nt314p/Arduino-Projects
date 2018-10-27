char val = 0;

int ledpin = 6;
void setup() {
  pinMode(ledpin, OUTPUT);
  digitalWrite(ledpin, LOW);
  delay(1000);
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  Serial.begin(9600);
}

void loop() {
  if (Serial.available()) {
    Serial.println(Serial.read());
  }

  if (val == 0) {
    digitalWrite(ledpin, LOW);
    delay(2000);
    Serial.println("LED off");
  }

  if (val == 1) {
    digitalWrite(ledpin, HIGH);
    delay(2000);
    Serial.println("LED on");
  }

}

