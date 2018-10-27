#include <LiquidCrystal.h>

const byte buzzerPin = 3;
const byte sensorPin = 2;

const byte buttonPins[10] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13}; // index position corresponds to button value
bool buttonStates[10];
bool prevStates[10];

bool first = true;
bool alarmOn = false;

byte pass[4] = {10, 10, 10, 10};

LiquidCrystal lcd(14, 15, 16, 17, 18, 19);

void startMessage() {
  lcd.clear();
  lcd.begin(16, 2);
  lcd.print("NTONGSP3 SECURE");
  delay(1000);
  setCode();
}

void setCode() {
  lcd.clear();
  lcd.print("SET PASSCODE:");
  lcd.setCursor(0, 1);
  lcd.blink();
}

void alarm() {
  digitalWrite(buzzerPin, HIGH);
}

void passcode() {
  lcd.blink();
  int count = 0;
  int wrongNum = 0;
  while (count < 4) {
    for (int i = 0; i < 10; i++) {
      updateButtonStates();
      if (buttonStates[i]) {
        delay(20);
        updateButtonStates();
        if (buttonStates[i]) {
          tone(buzzerPin, 523, 100);
          while (buttonStates[i]) {
            updateButtonStates();
          }
          lcd.print(i);
          if (pass[count] == 10) {
            pass[count] = i;
          } else if (pass[count] == i) {
          } else if (pass[count] != i) {
            wrongNum++;
          }
          count++;
        }
      }
    }
  }
  lcd.setCursor (0, 0);
  lcd.noBlink();
  if (first) {
    lcd.clear();
    lcd.print("CODE SET!");
    lcd.setCursor(0, 1);
    for (int i = 0; i < 4; i++) {
      lcd.print(pass[i]);
    }
    first = false;
    delay(2000);
    lcd.clear();
  } else if (wrongNum < 1) {
    if (alarmOn) {
      lcd.print("ALARM DISARMED!");
      digitalWrite(buzzerPin, LOW);

      delay(100);
      tone(buzzerPin, 202);
      delay(220);
      noTone(buzzerPin);
      delay(10);
      tone(buzzerPin, 523);
      delay(220);
      noTone(buzzerPin);
      delay(10);
      tone(buzzerPin, 1047);
      delay(220);
      noTone(buzzerPin);
      delay(600);
      lcd.clear();
      alarmOn = false;
    } else { // reset password
      while (buttonStates[0]) {}
      lcd.clear();
      lcd.print("0 TO RESET CODE");
      lcd.setCursor(0, 1);
      lcd.print("1 TO EXIT");
      while (buttonStates[0]) {
        updateButtonStates();
      }

      if (buttonStates[0]) {
        delay(100);
        first = true;
        setCode();
      } else if (buttonStates
    }
} else {
  lcd.print("WRONG PASSCODE!");

    delay(100);
    for (int i = 0; i < 3; i++) {
      tone(buzzerPin, 131);
      delay(200);
      noTone(buzzerPin);
      delay(100);
    }

    delay(600);
    lcd.clear();
  }
}

void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);

  startMessage();
  delay(1000);

  // initializing button pins
  for (int i = 0; i < 10; i++) {
    pinMode(buttonPins[i], INPUT);
  }

  pinMode(buzzerPin, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(sensorPin), alarm, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:

  updateButtonStates();
  printButtonStates();

  //lcdButtonStates();

  lcd.setCursor(0, 1);
  passcode();

  if (alarmOn) {
    alarm();
  }

}

void updateButtonStates() {
  for (int i = 0; i < 10; i++) {
    buttonStates[i] = digitalRead(buttonPins[i]);
  }
}

void printButtonStates() {

  if (!compareArrays(prevStates, buttonStates)) {
    Serial.println("------------------");
    for (int i = 0; i < 10; i++) {
      Serial.print(i);
      Serial.print(": ");
      Serial.println(digitalRead(buttonPins[i]));
    }

    memcpy(prevStates, buttonStates, 10 * sizeof(bool));
  }
}

void lcdButtonStates() {
  lcd.clear();

  for (int i = 0; i < 10; i++) {
    if (buttonStates[i]) {
      lcd.print(i);
    } else {
      lcd.print(" ");
    }
  }
  delay(10);
}

boolean compareArrays(bool *a, bool *b) {
  for (int n = 0; n < 10; n++) if (a[n] != b[n]) return false;
  return true;
}













