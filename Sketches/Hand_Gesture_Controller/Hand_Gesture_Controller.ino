
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
  if (bounds(usonicR.dist, 35, 50) && bounds(usonicR.dist, 35, 50)) {
    Serial.println("PLAY/PAUSE");
    delay(500);
  }

  // LEFT LOCK
  if (bounds(usonicL.dist, 12, 22)) {
    delay(100);
    usonicL.calculateDist();
    if (bounds(usonicL.dist, 12, 22)) {
      Serial.println("LEFT LOCKED");
      while (usonicL.dist <= 35) {
        usonicL.calculateDist();
        if (usonicL.dist < 10) {
          Serial.println("VOL UP");
          delay(300);
        }
        if (usonicL.dist > 24) {
          Serial.println("VOL DOWN");
          delay(300);
        }
      }
    }
  }

  // RIGHT LOCK
  if (bounds(usonicR.dist, 12, 22)) {
    delay(100);
    usonicR.calculateDist();
    if (bounds(usonicR.dist, 12, 22)) {
      Serial.println("RIGHT LOCKED");
      while (usonicR.dist <= 35) {
        usonicR.calculateDist();
        if (usonicR.dist < 10) {
          Serial.println("FORWARD");
          delay(300);
        }
        if (usonicR.dist > 24) {
          Serial.println("REWIND");
          delay(300);
        }
      }
    }
  }
  delay(200);
}


bool bounds (double n, int mn, int mx) {
  if (n > mn && n < mx) {
    return true;
  }
  return false;
}

