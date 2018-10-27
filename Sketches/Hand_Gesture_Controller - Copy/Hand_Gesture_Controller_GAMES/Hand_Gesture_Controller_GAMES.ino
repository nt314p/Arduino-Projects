bool toggle = false;

class Ultrasonic {
    int trigPin;
    int echoPin;


  public:
    double dist;
    Ultrasonic(int t, int e) {
      trigPin = t;
      echoPin = e;
      dist = 0;
      pinMode(trigPin, OUTPUT);
      pinMode(echoPin, INPUT);
    }

    double calculateDist() {
      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);
      dist = pulseIn(echoPin, HIGH) * 0.0343 / 2;

      if (dist > 50) dist = 50.0;
      return dist;
    }
};

Ultrasonic usonicR(3, 4);
Ultrasonic usonicL(5, 6);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  usonicR.calculateDist();
  usonicL.calculateDist();

  // PAUSE / PLAY
//  if (bounds(usonicR.dist, 35, 50) && bounds(usonicR.dist, 35, 50)) {
//    Serial.println("PLAY/PAUSE");
//    //delay(500);
//    toggle = !toggle;
//  }

  while (bounds(usonicL.dist, 5, 35)) {
    usonicL.calculateDist();
    if (usonicL.dist < 15) {
      Serial.println("MLEFT");
      delay(50);
    }
    if (usonicL.dist > 15) {
      Serial.println("MRIGHT");
      delay(50);
    }
  }

  while (bounds(usonicR.dist, 5, 35)) {
    usonicR.calculateDist();
    if (usonicR.dist < 15) {
      Serial.println("MUP");
      delay(50);
    }
    if (usonicR.dist > 15) {
      Serial.println("SHOOT");
      delay(50);
    }
  }

  delay(80);
}


bool bounds (double n, int mn, int mx) {
  if (n > mn && n < mx) {
    return true;
  }
  return false;
}

