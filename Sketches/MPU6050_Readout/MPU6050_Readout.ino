#pragma GCC optimize("-O3")

#include <Wire.h>

#define MPU 0x68
#define MPU_GYRO_XOUT 0x43
#define MPU_ACCEL_XOUT 0x3B
#define MPU_GYRO_CONFIG 0x1B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_PWR_MGMT_1 0x6B
#define MPU_DEVICE_RESET 0b00010001  // bit banging is MSB first
#define MPU_DEVICE_SLEEP 0b00010010

//                          +- deg/sec
const uint8_t FS_SEL_0 = 0;  //   250
const uint8_t FS_SEL_1 = 1;  //   500
const uint8_t FS_SEL_2 = 2;  //  1000
const uint8_t FS_SEL_3 = 3;  //  2000

//                           +- gs
const uint8_t AFS_SEL_0 = 0;  //   2
const uint8_t AFS_SEL_1 = 1;  //   4
const uint8_t AFS_SEL_2 = 2;  //   8
const uint8_t AFS_SEL_3 = 3;  //  16

const uint8_t BLE_PWR_PIN = 2;
const uint8_t MOUSE_L_PIN = 5;
const uint8_t MOUSE_R_PIN = 6;
const uint8_t MOUSE_M_PIN = 3;
const uint8_t CHARGE_KEY_PIN = 4;
const uint8_t MPU_PWR_PIN = 7;

const unsigned long BLE_BAUD = 38400;
const uint8_t SIGNATURE = 0b10101000;

const float SCALE_FACTORS[] = { 131, 65.5, 32.8, 16.4 };
const int DEGREES_RANGE[] = { 250, 500, 1000, 2000 };
const int ACCEL_SCALE_FACTORS[] = { 16384, 8192, 4096, 2048 };
const int ACCEL_RANGE[] = { 2, 4, 8, 16 };

//#define BLE_DEBUG // Access to BLE AT commands without using software serial

// TODO: is it possible to remove floating point ops?
// could result in speed up and allow for more corrections on uC

struct Vector3 {
  float x;
  float y;
  float z;
};

struct Vector3Int16 {
  int16_t x;
  int16_t y;
  int16_t z;
};

struct Packet {
  Vector3Int16 gyro;
  uint8_t buttonData;
};

inline Vector3 add(Vector3 a, Vector3 b) __attribute__((always_inline));
inline Vector3 multiply(Vector3 v, float scalar) __attribute__((always_inline));

const Vector3 zero = { 0, 0, 0 };
Vector3 currGyro = zero;
Vector3Int16 currGyroInt16 = { 0, 0, 0 };
Vector3 prevGyro = zero;

Packet packet = { { 0, 0, 0 }, SIGNATURE };

// gyro and accelerometer precision
const int FS_SEL = FS_SEL_1;
const int AFS_SEL = AFS_SEL_1;

const float LSB_PER_DEG_PER_SEC = SCALE_FACTORS[FS_SEL];
const float LSB_PER_G = ACCEL_SCALE_FACTORS[AFS_SEL];
const int MAX_DEGREES = DEGREES_RANGE[FS_SEL];

// { -565, -248, -42 }
const Vector3Int16 GYRO_ERROR_Int16 = { -546, -239, -41 };
Vector3 gyroError = { -8.623f, -3.785f, -0.642f };
Vector3 accelError = zero;

volatile bool timerFlag = false;
unsigned long currentMs;
unsigned long diffUs = 0;
unsigned long previousUs = 0;

// the time (in ms) when the last movement was recorded
// a movement has an angular velocity greater than the threshold (in any axis)
unsigned long lastMovementTimeMs;
const float DEG_PER_SEC_MOVEMENT_THRESHOLD = 10.0f;
const int ANG_VEL_MOVEMENT_THRESHOLD = DEG_PER_SEC_MOVEMENT_THRESHOLD / MAX_DEGREES * 0x7FFF;
const int SLEEP_TIME_MINS = 4;  // sleep after x minutes of no movement

volatile bool awake = false;
int previousMiddleButton = LOW;
bool isBLEPowered = false;

void setup() {
  pinMode(CHARGE_KEY_PIN, OUTPUT);
  digitalWrite(CHARGE_KEY_PIN, HIGH);

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(MOUSE_L_PIN, INPUT_PULLUP);
  pinMode(MOUSE_R_PIN, INPUT_PULLUP);
  pinMode(MOUSE_M_PIN, INPUT);

  pinMode(BLE_PWR_PIN, OUTPUT);
  pinMode(MPU_PWR_PIN, OUTPUT);

  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  digitalWrite(BLE_PWR_PIN, LOW);
  digitalWrite(MPU_PWR_PIN, LOW);

  Serial.begin(BLE_BAUD);
  Serial.println("Started");

  powerOnMPU();
  delay(50);

  cli();  // stop interrupts

  TCCR1A = 0;  // clear timer registers
  TCCR1B = 0;
  TCNT1 = 0;  // initialize counter value to 0

  // 10 ms intervals
  OCR1A = 19999;  // 0.01 / (8 / 16e6) = 20000

  TCCR1B |= (1 << WGM12);  // turn on CTC mode
  TCCR1B |= (1 << CS11);   // f_cpu/8 prescaler
  TIMSK1 = (1 << OCIE1A);  // enable timer compare interrupt

  sei();  // enable interrupts

  currentMs = millis();
  lastMovementTimeMs = currentMs;
}

ISR(TIMER1_COMPA_vect) {
  timerFlag = true;
}

void loop() {
  currentMs = millis();

  // heartbeat
  uint8_t lit = ((currentMs & 0x3FF) < 10) ? 0b00100000 : 0;
  PORTB = (PORTB & 0b11011111) | lit;  // clear bit, then set

#ifdef BLE_DEBUG
  doBLECommands();
  return;
#endif

  int currentMiddleButton = digitalRead(MOUSE_M_PIN);
  if (previousMiddleButton == HIGH && currentMiddleButton == LOW) {
    Serial.println("Button pressed, turning on");
    lastMovementTimeMs = currentMs;
    awake = true;
  }
  
  previousMiddleButton = currentMiddleButton;

  if (awake && currentMs - lastMovementTimeMs > 1000L * 60 * SLEEP_TIME_MINS) {
    Serial.println("Sleep time exceeded, turning off");
    awake = false;
    powerOffBLE();
    powerOffMPU();
  }

  if (!awake) return;

  if (!isBLEPowered) {
    isBLEPowered = true;
    powerOnBLE();
    powerOnMPU();
  }

  readGyroInt16();  // 172 us
  
  // check for movement
  if (currGyroInt16.x > ANG_VEL_MOVEMENT_THRESHOLD) {
    lastMovementTimeMs = currentMs;
  }

  /*
  if (digitalRead(MOUSE_L_PIN) == LOW) {  // testing for sleep functionality
    powerOffDevice();
  }*/


  unsigned long currentUs = micros();
  diffUs = currentUs - previousUs;
  previousUs = currentUs;

  //if (currentMs % 1000 == 0) Serial.println(diffUs);

  if (timerFlag) {
    sendData();  // 52 us
    timerFlag = false;
  }

  prevGyro = currGyro;
}

void sendData() {
  Serial.write((uint8_t*)&currGyroInt16, sizeof(currGyroInt16));

  //Vector3 gyroSum = add(currGyro, prevGyro);
  //gyroSum = multiply(gyroSum, 0.5f);

  //Vector3 accel = readAccel();

  uint8_t buttonData = SIGNATURE | ((digitalRead(MOUSE_M_PIN) == LOW) << 2) | ((digitalRead(MOUSE_L_PIN) == LOW) << 1) | (digitalRead(MOUSE_R_PIN) == LOW);
  Serial.write(buttonData);
}

void powerOffDevice() {  // double pulse is sufficient to power off
  while (1) {
    digitalWrite(CHARGE_KEY_PIN, LOW);
    delay(120);
    digitalWrite(CHARGE_KEY_PIN, HIGH);
    delay(120);
  }
}

// bluetooth setup commands
const char* setupCommands[] = {
  "NAMERemy",  // *Remy*
  "NOTI1",     // enable connect/disconnect notification
  "TYPE3",     // auth and bond
  "PASS802048",
  "POWE1",  // -6dbm
  "COMI0",  // min connection interval 7.5 ms
  "COMA0",  // max connection interval 7.5 ms
  "ADVI2",  // advertising interval 211.25 ms
  "BAUD2"   // set baud rate to 38400 bps
};

void setupBle() {  // iterate through setup commands and exec
  return;

  digitalWrite(BLE_PWR_PIN, HIGH);
  delay(2000);

  Serial.begin(9600);
  Serial.print("AT+NAMERemy");
  delay(100);
  Serial.print("AT+NOTI1");
  delay(100);
  Serial.print("AT+POWE1");
  delay(100);
  Serial.print("AT+ADVI4");
  delay(100);
  Serial.print("AT+BAUD2");
  delay(100);
}

void powerOnBLE() {
  isBLEPowered = true;
  digitalWrite(BLE_PWR_PIN, HIGH);
  //Serial.begin(BLE_BAUD);
}

void powerOffBLE() {
  isBLEPowered = false;
  digitalWrite(BLE_PWR_PIN, LOW);
  //Serial.end();
  //pinMode(0, OUTPUT);
  //digitalWrite(0, LOW);
}

void powerOnMPU() {
  digitalWrite(MPU_PWR_PIN, HIGH);
  delay(10);
  setupMPU();
}

void powerOffMPU() {
  digitalWrite(MPU_PWR_PIN, LOW);
}

void setupMPU() {
  Wire.begin();
  Wire.end();
  delay(10);
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(MPU_PWR_MGMT_1);    // select the power management register
  Wire.write(MPU_DEVICE_RESET);  // reset device
  Wire.endTransmission(true);
  delay(1);
  setGyroConfig();
  setAccelConfig();
}

void setGyroConfig() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(MPU_GYRO_CONFIG);
  Wire.write(FS_SEL << 3);
  Wire.endTransmission(true);
}

void setAccelConfig() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(MPU_ACCEL_CONFIG);
  Wire.write(AFS_SEL << 3);
  Wire.endTransmission(true);
}

void readGyro() {
  Wire.beginTransmission(MPU);
  Wire.setClock(1000000);
  Wire.write(MPU_GYRO_XOUT);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);                                                     // read the next six registers starting at MPU_GYRO_XOUT
  currGyro.x = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC + gyroError.x;  // TODO: avoid conversion to float
  currGyro.y = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC + gyroError.y;
  currGyro.z = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC + gyroError.z;
}

void readGyroInt16() {
  Wire.beginTransmission(MPU);
  Wire.setClock(1000000);
  Wire.write(MPU_GYRO_XOUT);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);  // read the next six registers starting at MPU_GYRO_XOUT
  currGyroInt16.x = (Wire.read() << 8 | Wire.read()) + GYRO_ERROR_Int16.x;
  currGyroInt16.y = (Wire.read() << 8 | Wire.read()) + GYRO_ERROR_Int16.y;
  currGyroInt16.z = (Wire.read() << 8 | Wire.read()) + GYRO_ERROR_Int16.z;
}

Vector3 readAccel() {
  Wire.beginTransmission(MPU);
  Wire.setClock(1000000);
  Wire.write(MPU_ACCEL_XOUT);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);
  float accelX = (Wire.read() << 8 | Wire.read()) / LSB_PER_G + accelError.x;  // TODO: see if error implementation is necessary
  float accelY = (Wire.read() << 8 | Wire.read()) / LSB_PER_G + accelError.y;
  float accelZ = (Wire.read() << 8 | Wire.read()) / LSB_PER_G + accelError.z;
  return { accelX, accelY, accelZ };
}

Vector3 add(Vector3 a, Vector3 b) {
  return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 multiply(Vector3 v, float scalar) {
  return { v.x * scalar, v.y * scalar, v.z * scalar };
}

void debugFlash() {
  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
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

    while (input != '\n') {  // read message into buffer
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
    delay(700);

    Serial.begin(BLE_BAUD);
    Serial.print((char*)buffer);  // send command to BLE module
    isArduinoSender = false;
  } else {
    if (!Serial.available()) return;
    delay(50);  // wait for message to get sent from BLE

    index = 0;

    while (Serial.available() > 0) {  // read message into buffer
      buffer[index] = Serial.read();
      index++;
    }

    buffer[index] = 0;

    digitalWrite(BLE_PWR_PIN, LOW);
    delay(50);

    Serial.print("\nFrom BLE: ");  // display message from BLE module to user
    Serial.print((char*)buffer);
    isArduinoSender = true;
  }
}
#endif
