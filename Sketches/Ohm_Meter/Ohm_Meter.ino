float r;

void setup() {
  pinMode(A0, INPUT);
  Serial.begin(9600);
}

void loop () {
  float v = analogRead(A0);
  v *= 5.0/1023.0;
  r = (47000.0*v)/(5.0-v);
  Serial.print("Ohms: ");
  Serial.print(r,3);
  Serial.print(" Volts: ");
  Serial.println(v,3);
  delay(200);
}

