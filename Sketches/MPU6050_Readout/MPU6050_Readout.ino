#pragma GCC optimize("-O3")

#include <Wire.h>

#define MPU 0x68
#define GYRO_X 0x43
#define ACCEL_X 0x3B
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define MPU_PWR_MGMT_1 0x6B
#define MPU_DEVICE_RESET 0b00010001 // bit banging is MSB first
#define MPU_DEVICE_SLEEP 0b00010010

//               +- deg/sec
#define FS_SEL_0 0 //   250
#define FS_SEL_1 1 //   500
#define FS_SEL_2 2 //  1000
#define FS_SEL_3 3 //  2000

//                   +- gs
#define AFS_SEL_0 0 //   2
#define AFS_SEL_1 1 //   4
#define AFS_SEL_2 2 //   8
#define AFS_SEL_3 3 //  16

#define BLE_PWR_PIN 2
#define MOUSE_L_PIN 5
#define MOUSE_R_PIN 6
#define MOUSE_M_PIN 3
#define CHARGE_KEY_PIN 4
#define MPU_PWR_PIN 7

#define gyroSmoothingLen 10
#define calibrationIterations 5000

// TODO: SCALE DOES NOT WORK?

struct Vector3 {
  float x;
  float y;
  float z;
};

inline Vector3 add(Vector3 a, Vector3 b) __attribute__((always_inline));
inline Vector3 multiply(Vector3 v, float scalar) __attribute__((always_inline));
inline void mapVecToShortArr(short shorts[], Vector3 v, float range) __attribute__((always_inline));

const float SCALE_FACTORS[] = {131, 65.5, 32.8, 16.4};
const float DEGREES_RANGE[] = {250, 500, 1000, 2000};
const float A_SCALE_FACTORS[] = {16384, 8192, 4096, 2048};
const float ACCEL_RANGE[] = {2, 4, 8, 16};
const byte signature = 0b10101000;

int gyroSmoothingCount = 0;
const Vector3 zero = {0, 0, 0};
Vector3 currGyro = zero;
Vector3 gyroSmoothingBuffer[] = {zero, zero, zero, zero, zero, zero, zero, zero, zero, zero};
short shortBuffer[3];

// gyro and accelerometer precision
const int FS_SEL = FS_SEL_1;
const int AFS_SEL = AFS_SEL_1;

const float LSB_PER_DEG_PER_SEC = SCALE_FACTORS[FS_SEL];
const float LSB_PER_G = A_SCALE_FACTORS[AFS_SEL];
const int MAX_DEGREES = DEGREES_RANGE[FS_SEL];

Vector3 gyroError = { -8.643, -3.785f, -0.642f }; //zero;
Vector3 accelError = zero;

volatile bool timerFlag = false;
unsigned long diff = 0;
unsigned long prevMicros = 0;

// the time (in ms) when the last movement was recorded
// a movement has an angular velocity greater than the threshold (in any axis)
unsigned long lastMovementTimeMs;
const float degPerSecMovementThreshold = 3.5;

volatile bool awake = false;
bool isBLEPowered = false;

void setup() {
  pinMode(13, OUTPUT);
  pinMode(MOUSE_L_PIN, INPUT_PULLUP);
  pinMode(MOUSE_R_PIN, INPUT_PULLUP);
  pinMode(MOUSE_M_PIN, INPUT);
  pinMode(BLE_PWR_PIN, OUTPUT);
  pinMode(MPU_PWR_PIN, OUTPUT);
  pinMode(CHARGE_KEY_PIN, OUTPUT);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  digitalWrite(BLE_PWR_PIN, LOW);
  digitalWrite(MPU_PWR_PIN, LOW);
  digitalWrite(CHARGE_KEY_PIN, HIGH);

  Serial.begin(38400);
  Serial.println("Started");

  powerOnMPU();
  delay(50);
  //calibrateGyro();
  //delay(10);
  //calibrateAccel();
  //delay(10);

  cli(); // stop interrupts

  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1 = 0; // initialize counter value to 0

  // 10 ms intervals
  OCR1A = 19999; // 0.01/(8/16e6) = 20000
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11); // f_cpu/8 prescaler
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  sei(); // enable interrupts

  attachInterrupt(digitalPinToInterrupt(MOUSE_M_PIN), onMiddleBtnDown, RISING);

  lastMovementTimeMs = millis();
}

ISR(TIMER1_COMPA_vect) {
  timerFlag = true;
}

void onMiddleBtnDown() {
  lastMovementTimeMs = millis();
  awake = true;
}

void loop() {
  unsigned long currMs = millis();

  // heartbeat
  byte lit = (currMs % 1000 < 8) ? 1 : 0;
  PORTB |= lit << 5; // pin 13
  PORTB &= 0b11011111 | (lit << 5);

  if (currMs - lastMovementTimeMs > 300 * 1000) {
    awake = false;
    //powerOffBLE();
    powerOffMPU();
  }

  if (!awake) return;
  if (!isBLEPowered) {
    isBLEPowered = true;
    //powerOnBLE();
    //powerOnMPU();
  }

  readGyro(&currGyro);
  gyroSmoothingBuffer[gyroSmoothingCount] = currGyro;

  // check for movement
  if (abs(currGyro.x) > degPerSecMovementThreshold ||
      abs(currGyro.y) > degPerSecMovementThreshold ||
      abs(currGyro.z) > degPerSecMovementThreshold) {
    lastMovementTimeMs = currMs;
    Serial.println("Movement");
  }

  gyroSmoothingCount++;
  if (gyroSmoothingCount >= gyroSmoothingLen) gyroSmoothingCount = 0;

  if (digitalRead(MOUSE_L_PIN) == LOW) { // testing for sleep functionality
    powerOffDevice();
  }

  unsigned long currUs = micros();

  if (currMs % 1000 == 0)
    Serial.println(currUs - prevMicros);

  prevMicros = currUs;

  return;
  if (timerFlag) {
    unsigned long currMicros = micros();
    diff = currMicros - prevMicros;
    prevMicros = currMicros;
    sendData();
    timerFlag = false;
  }
}

void sendData() {
  Vector3 gyroSum = zero;
  for (int i = 0; i < gyroSmoothingLen; i++) {
    gyroSum = add(gyroSum, gyroSmoothingBuffer[i]);
  }

  gyroSum = multiply(gyroSum, 1.0 / gyroSmoothingLen);

  Vector3 accel = readAccel();

  mapVecToShortArr(shortBuffer, gyroSum, MAX_DEGREES);
  Serial.write((byte*) &shortBuffer, 3 * sizeof(short));

  byte buttonData = signature;
  buttonData += ((digitalRead(MOUSE_M_PIN) == HIGH) << 2) + ((digitalRead(MOUSE_L_PIN) == LOW) << 1) + (digitalRead(MOUSE_R_PIN) == LOW);
  Serial.write(buttonData);
}

void powerOffDevice() {
  digitalWrite(CHARGE_KEY_PIN, LOW);
  delay(80);
  digitalWrite(CHARGE_KEY_PIN, HIGH);
  delay(80);
  digitalWrite(CHARGE_KEY_PIN, LOW);
  delay(80);
  digitalWrite(CHARGE_KEY_PIN, HIGH);
}

const char* setupCommands[] = {
  "NAMERemy", // *Remy*
  "NOTI1", // enable connect/disconnect notification
  "TYPE3", // auth and bond
  "PASS802048",
  "POWE1", // -6dbm
  "COMI0", // min connection interval 7.5 ms
  "COMA0", // max connection interval 7.5 ms
  "ADVI2", // advertising interval 211.25 ms
  "BAUD2" // set baud rate to 38400 bps
};

void setupBle() { // iterate through setup commands and exec
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
  Serial.begin(38400);
}

void powerOffBLE() {
  isBLEPowered = false;
  digitalWrite(BLE_PWR_PIN, LOW);
  //Serial.end();
  //pinMode(0, OUTPUT);
  //digitalWrite(0, LOW);
}

// converts a vector3 into a byte array of length 6, with every two bytes representing a short
// this short is the mapping of -range, +range (a float) to -short.MAX_VALUE, short.MAX_VALUE (32767)
void mapVecToShortArr(short shorts[], Vector3 v, float range) {
  shorts[0] = (short) (32767 * v.x / range);
  shorts[1] = (short) (32767 * v.y / range);
  shorts[2] = (short) (32767 * v.z / range);
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
  Wire.beginTransmission(MPU);
  Wire.write(MPU_PWR_MGMT_1); // select the power management register
  Wire.write(MPU_DEVICE_RESET); // reset device
  Wire.endTransmission(true);
  delay(1);
  setGyroConfig();
  setAccelConfig();
}

void setGyroConfig() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(GYRO_CONFIG);
  Wire.write(FS_SEL << 3);
  Wire.endTransmission(true);
}

void setAccelConfig() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(ACCEL_CONFIG);
  Wire.write(AFS_SEL << 3);
  Wire.endTransmission(true);
}

/*
  Vector3 calibrateGyro() {
  Serial.print(gyroError.x);
  Serial.print('\t');
  Serial.print(gyroError.y);
  Serial.print('\t');
  Serial.println(gyroError.z);
  Vector3 errorSum = zero;
  for (int i = 0; i < calibrationIterations; ++i) {
    errorSum = add(errorSum, readGyro());
    delay(2);
  }
  gyroError = multiply(errorSum, -1.0 / calibrationIterations);
  }*/

Vector3 calibrateAccel() {
  Vector3 errorSum = zero;
  for (int i = 0; i < calibrationIterations; ++i) {
    errorSum = add(errorSum, readAccel());
    delay(2);
  }
  errorSum = add(errorSum, { 0, 0, -calibrationIterations} ); // subtract off gravity (1g)
  accelError = multiply(errorSum, -1.0 / calibrationIterations);

  Serial.print(accelError.x, 5);
  Serial.print('\t');
  Serial.print(accelError.y, 5);
  Serial.print('\t');
  Serial.println(accelError.z, 5);
}

void readGyro(Vector3* vec) {
  Wire.beginTransmission(MPU);
  Wire.setClock(1000000);
  Wire.write(GYRO_X);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true); // read the next six registers starting at GYRO_X
  vec->x = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC + gyroError.x;
  vec->y = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC + gyroError.y;
  vec->z = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC + gyroError.z;
}

Vector3 readAccel() {
  Wire.beginTransmission(MPU);
  Wire.setClock(1000000);
  Wire.write(ACCEL_X);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);
  float accelX = (Wire.read() << 8 | Wire.read()) / LSB_PER_G;
  float accelY = (Wire.read() << 8 | Wire.read()) / LSB_PER_G;
  float accelZ = (Wire.read() << 8 | Wire.read()) / LSB_PER_G;
  return add({accelX, accelY, accelZ}, accelError);
}

Vector3 add(Vector3 a, Vector3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 multiply(Vector3 v, float scalar) {
  return {v.x * scalar, v.y * scalar, v.z * scalar};
}
