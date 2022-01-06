#include <Wire.h>
#include <SoftwareSerial.h>

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

#define MOUSE_L_PIN 5
#define MOUSE_R_PIN 6
#define MOUSE_M_PIN 7
#define MOUSE_SCROLL_PIN A0
#define MOUSE_SCROLL_MIN 40

#define gyroSmoothingLen 10
#define scrollSmoothingLen 30
#define calibrationIterations 100

// TODO: SCALE DOES NOT WORK?

typedef struct {
  float x;
  float y;
  float z;
} Vector3;

const float SCALE_FACTORS[] = {131, 65.5, 32.8, 16.4};
const float DEGREES_RANGE[] = {250, 500, 1000, 2000};
const float A_SCALE_FACTORS[] = {16384, 8192, 4096, 2048};
const float ACCEL_RANGE[] = {2, 4, 8, 16};
const byte signature = 0b10101000;

int gyroSmoothingCount = 0;
Vector3 zero = {0, 0, 0};
Vector3 gyroSmoothingBuffer[] = {zero, zero, zero, zero, zero, zero, zero, zero, zero, zero};
short shortBuffer[3];

int scrollSmoothingCount = 0;
int scrollSmoothingBuffer[scrollSmoothingLen];

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

int prevScroll = 0;
int currScroll;
int deltaScroll;

void setup() {
  pinMode(13, OUTPUT);
  pinMode(MOUSE_L_PIN, INPUT);
  pinMode(MOUSE_R_PIN, INPUT);
  pinMode(MOUSE_M_PIN, INPUT);
  pinMode(MOUSE_SCROLL_PIN, INPUT);

  Serial.begin(38400); // for bluetooth module HM-11

  resetMPU6050();
  delay(1);
  calibrateGyro();
  delay(1);
  calibrateAccel();
  delay(1);

  //  Serial.print("AT+BAUD1\r\n");
  //  delay(100);

  //  Serial.print("AT\r\n");
  //  delay(200);
  //  Serial.print("AT+VERSION?\r\n");
  //  delay(200);
  //  Serial.print("AT+NAME?\r\n");
  //  delay(200);
  //  Serial.print("AT+UART?\r\n");
  //  delay(200);
  //  Serial.print("AT+PSWD?\r\n");

  cli(); // stop interrupts

  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1  = 0; // initialize counter value to 0

  // 10 ms intervals
  OCR1A = 19999; // 0.01/(8/16e6) = 20000
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11); // f_cpu/8 prescaler
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  sei(); // enable interrupts
}

ISR(TIMER1_COMPA_vect) {
  timerFlag = true;
}
void loop() {
  //  btSerial.write(255);
  //  return;
  //  while (btSerial.available())
  //    Serial.write(btSerial.read());
  //
  //  while (Serial.available())
  //    btSerial.write(Serial.read());
  //
  //  return;

  digitalWrite(13, (millis() % 1000) < 500);

  gyroSmoothingBuffer[gyroSmoothingCount] = readGyro();
  gyroSmoothingCount = (gyroSmoothingCount + 1) % gyroSmoothingLen;

  scrollSmoothingBuffer[scrollSmoothingCount] = analogRead(MOUSE_SCROLL_PIN);
  scrollSmoothingCount = (scrollSmoothingCount + 1) % scrollSmoothingLen;

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
  Serial.write((byte*) &shortBuffer, 3* sizeof(short));

  currScroll = 0;
  for (int i = 0; i < scrollSmoothingLen; i++) {
    currScroll += scrollSmoothingBuffer[i];
  }
  currScroll /= scrollSmoothingLen;

  bool isScrollPressed = currScroll > MOUSE_SCROLL_MIN;

  if (isScrollPressed) {
    if (prevScroll == -1) {
      prevScroll = currScroll;
    }
    deltaScroll = currScroll - prevScroll;
    prevScroll = currScroll;
  } else {
    prevScroll = -1;
  }
  deltaScroll = 0;
  Serial.write(deltaScroll);

  byte buttonData = signature;
  buttonData += ((digitalRead(MOUSE_M_PIN) == HIGH) << 2) + ((digitalRead(MOUSE_L_PIN) == HIGH) << 1) + (digitalRead(MOUSE_R_PIN) == HIGH);
  Serial.write(buttonData);
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
