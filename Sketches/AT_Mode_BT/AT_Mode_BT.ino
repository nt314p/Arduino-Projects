#define AT_PIN 2
#define BT_EN_PIN 3

void setup() {
  pinMode(AT_PIN, OUTPUT);
  pinMode(BT_EN_PIN, OUTPUT);
  digitalWrite(BT_EN_PIN, LOW);
  digitalWrite(AT_PIN, HIGH);
  delay(10);
  digitalWrite(BT_EN_PIN, HIGH);
  
  Serial.begin(38400);

  delay(100);
  Serial.print("AT\r\n");
  delay(100);
  Serial.print("AT+VERSION?\r\n");
  delay(100);
  Serial.print("AT+NAME?\r\n");
  delay(100);
  Serial.print("AT+UART?\r\n");
  delay(100);
  Serial.print("AT+PSWD?\r\n");
}

void loop() {
  while (Serial.available() > 0) {
    Serial.print((char) Serial.read());
  }
}
