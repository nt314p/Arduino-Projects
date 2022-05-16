#include <Wire.h>

// ---------
/*
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}*/


// ---------

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

#define gyroSmoothingLen 10
#define calibrationIterations 100
#define sequentialSimilarGyroReadsThreshold 200
#define simGyroTol 0.02


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
Vector3 zero = {0, 0, 0};
Vector3 gyroSmoothingBuffer[] = {zero, zero, zero, zero, zero, zero, zero, zero, zero, zero};
short shortBuffer[3];

int FS_SEL = FS_SEL_1;
int AFS_SEL = AFS_SEL_1;

float LSB_PER_DEG_PER_SEC = SCALE_FACTORS[FS_SEL];
float LSB_PER_G = A_SCALE_FACTORS[AFS_SEL];
int MAX_DEGREES = DEGREES_RANGE[FS_SEL];

Vector3 gyroError = zero;
Vector3 accelError = zero;

volatile bool timerFlag = false;
unsigned long diff = 0;
unsigned long prevMicros = 0;

// the time (in ms) when the last movement was recorded
// a movement has an angular velocity greater than the threshold (in any axis)
unsigned long lastMovementTimeMs;

/*
    Live gyro calibration:

    We know that the gyro should be reporting similar nonzero values when not in motion.
    We identify these values by comparing a standard gyro value we believe represents
    the error (standardGyro) to a current gyro measurement. If the current gyro
    measurement agrees (within some percent tolerance) with the standard, we increment
    sequentialSimilarGyroReads. If the values to not agree, we reject the current
    standard and replace it with the newly read one. When we accumulate enough sequential
    similar reads, we recalibrate the gyro using our standard as the error.
*/
Vector3 standardGyro;
int sequentialSimilarGyroReads;

volatile bool awake = false;
bool isBLEPowered = false;

int prevScroll = 0;
int currScroll;
int deltaScroll;

void setup() {
  pinMode(13, OUTPUT);
  pinMode(MOUSE_L_PIN, INPUT_PULLUP);
  pinMode(MOUSE_R_PIN, INPUT_PULLUP);
  pinMode(MOUSE_M_PIN, INPUT);
  pinMode(BLE_PWR_PIN, OUTPUT);
  pinMode(CHARGE_KEY_PIN, OUTPUT);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  digitalWrite(BLE_PWR_PIN, LOW);
  digitalWrite(CHARGE_KEY_PIN, HIGH);

  Serial.begin(38400);
  Serial.println("Started");

  //Serial.println("AT+SLEEP\r\n");

  resetMPU6050();
  delay(1);
  calibrateGyro();
  delay(1);
  calibrateAccel();
  delay(1);

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
  awake = true;
}

void loop() {
  digitalWrite(13, (millis() % 1000) < 500); // heartbeat
  //Serial.println(millis());

  if (millis() - lastMovementTimeMs > 300 * 1000) {
    awake = false;
    powerOffBLE();
  }

  if (!awake) return;
  if (!isBLEPowered) {
    powerOnBLE();
  }

  //  btSerial.write(255);
  //  return;
  //  while (btSerial.available())
  //    Serial.write(btSerial.read());
  //
  //  while (Serial.available())
  //    btSerial.write(Serial.read());
  //
  //  return;

  Vector3 currGyro = readGyro();

  //Serial.println(gyroSmoothingCount);
//  Serial.print(currGyro.x);
//  Serial.print('\t');
//  Serial.print(currGyro.y);
//  Serial.print('\t');
//  Serial.println(currGyro.z);

//  float dx = abs(currGyro.x - standardGyro.x);
//  float dy = abs(currGyro.y - standardGyro.y);
//  float dz = abs(currGyro.z - standardGyro.z);

//  if (dx < standardGyro.x * simGyroTol && dy < standardGyro.y * simGyroTol && dz < standardGyro.z * simGyroTol) {
//    sequentialSimilarGyroReads++;
//
//    if (sequentialSimilarGyroReads >= sequentialSimilarGyroReadsThreshold) {
//      Serial.println("Calibrating...");
//      gyroError = { -standardGyro.x, -standardGyro.y, -standardGyro.z };
//    }
//  } else {
//    sequentialSimilarGyroReads = 0;
//    standardGyro = currGyro;
//  }

  gyroSmoothingBuffer[gyroSmoothingCount] = currGyro;

//  if (abs(currGyro.x) > 3 || abs(currGyro.y) > 3 || abs(currGyro.z) > 3) {
//    lastMovementTimeMs = millis();
//  }

  gyroSmoothingCount++;
  if (gyroSmoothingCount >= gyroSmoothingLen) gyroSmoothingCount = 0;

  if (digitalRead(MOUSE_L_PIN) == LOW) {
    digitalWrite(CHARGE_KEY_PIN, LOW);
    delay(80);
    digitalWrite(CHARGE_KEY_PIN, HIGH);
    delay(80);
    digitalWrite(CHARGE_KEY_PIN, LOW);
    delay(80);
    digitalWrite(CHARGE_KEY_PIN, HIGH);
  }

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

  //btSerial.write((byte*) &accel, sizeof(Vector3));
  mapVecToShortArr(shortBuffer, gyroSum, MAX_DEGREES);
  Serial.write((byte*) &shortBuffer, 3 * sizeof(short));

  byte buttonData = signature;
  buttonData += ((digitalRead(MOUSE_M_PIN) == HIGH) << 2) + ((digitalRead(MOUSE_L_PIN) == LOW) << 1) + (digitalRead(MOUSE_R_PIN) == LOW);
  Serial.write(buttonData);
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

void setupBle() {
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
  Serial.end();
  digitalWrite(BLE_PWR_PIN, LOW);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
}

// converts a vector3 into a byte array of length 6, with every two bytes representing a short
// this short is the mapping of -range, +range (a float) to -short.MAX_VALUE, short.MAX_VALUE (32767)
void mapVecToShortArr(short shorts[], Vector3 v, float range) {
  shorts[0] = (short) (32767 * v.x / range);
  shorts[1] = (short) (32767 * v.y / range);
  shorts[2] = (short) (32767 * v.z / range);
}

void resetMPU6050() {
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

Vector3 calibrateGyro() {
  Vector3 errorSum = zero;
  for (int i = 0; i < calibrationIterations; ++i) {
    errorSum = add(errorSum, readGyro());
    delay(1);
  }
  gyroError = multiply(errorSum, -1.0 / calibrationIterations);
}

Vector3 calibrateAccel() {
  Vector3 errorSum = zero;
  for (int i = 0; i < calibrationIterations; ++i) {
    errorSum = add(errorSum, readAccel());
    delay(1);
  }
  errorSum = add(errorSum, { 0, 0, -calibrationIterations} ); // subtract off gravity (1g)
  accelError = multiply(errorSum, -1.0 / calibrationIterations);
}

Vector3 readGyro() {
  Wire.beginTransmission(MPU);
  Wire.write(GYRO_X);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true); // read the next six registers starting at GYRO_X
  float gyroX = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC;
  float gyroY = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC;
  float gyroZ = (Wire.read() << 8 | Wire.read()) / LSB_PER_DEG_PER_SEC;
  return add({gyroX, gyroY, gyroZ}, gyroError);
}

Vector3 readAccel() {
  Wire.beginTransmission(MPU);
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

uint8_t * heapptr, * stackptr;
void check_mem() {
  stackptr = (uint8_t *)malloc(4);          // use stackptr temporarily
  heapptr = stackptr;                     // save value of heap pointer
  free(stackptr);      // free up the memory again (sets stackptr to 0)
  stackptr =  (uint8_t *)(SP);           // save value of stack pointer
}

int freeMem() {
  return (uint8_t)stackptr - (uint8_t) heapptr;
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
