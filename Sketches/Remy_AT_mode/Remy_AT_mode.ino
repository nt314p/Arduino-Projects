#define BLE_PWR_PIN 2
#define MOUSE_L_PIN 5
#define MOUSE_R_PIN 6
#define MOUSE_M_PIN 3
#define CHARGE_KEY_PIN 4
#define MPU_PWR_PIN 7

//#define BLE_DEBUG

void setup() {
  Serial.begin(38400);

  pinMode(CHARGE_KEY_PIN, OUTPUT);
  digitalWrite(CHARGE_KEY_PIN, HIGH);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MOUSE_L_PIN, INPUT_PULLUP);
  pinMode(MOUSE_R_PIN, INPUT_PULLUP);
  pinMode(MOUSE_M_PIN, INPUT);
  pinMode(BLE_PWR_PIN, OUTPUT);
  pinMode(MPU_PWR_PIN, OUTPUT);

  digitalWrite(BLE_PWR_PIN, LOW);
  digitalWrite(MPU_PWR_PIN, LOW);
}

void loop() {
  #ifdef BLE_DEBUG
  doBLECommands();
  return;
  #endif
}

#ifdef BLE_DEBUG
void doBLECommands() {
  static char buffer[64];
  static int index = 0;
  static bool isArduinoSender = true;

  if (isArduinoSender) {
    digitalWrite(BLE_PWR_PIN, LOW);

    if (!Serial.available()) return;
    delay(50);  // wait for full message
    index = 0;

    char input = Serial.read();

    while (input != '\n') {
      buffer[index] = input;
      index++;

      while (!Serial.available())
        ;

      input = Serial.read();
    }
    buffer[index] = 0;

    Serial.print("\nCommand: ");

    Serial.end();

    digitalWrite(BLE_PWR_PIN, HIGH);

    delay(800);

    Serial.begin(38400);

    Serial.print((char*)buffer);
    isArduinoSender = false;
  } else {
    if (!Serial.available()) return;
    delay(50);  // wait for message to get sent from BLE

    index = 0;

    while (Serial.available() > 0) {
      char input = Serial.read();
      buffer[index] = input;
      index++;
    }

    buffer[index] = 0;

    digitalWrite(BLE_PWR_PIN, LOW);
    delay(100);

    Serial.print("\nFrom BLE: ");
    Serial.print((char*)buffer);
    isArduinoSender = true;
  }
}
#endif