#include <Wire.h>

#define MPU 0x68
#define GYRO_X 0x43
#define ACCEL_X 0x3B
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define MPU_PWR_MGMT_1 0x6B
#define MPU_DEVICE_RESET 0b00000001 // bit banging is MSB first

//               +- deg/sec
#define FS_SEL_0 0 //   250
#define FS_SEL_1 1 //   500
#define FS_SEL_2 2 //  1000
#define FS_SEL_3 3 //  2000

//                   +- gs
#define AFS_SEL_0 0 //   2
#define AFS_SEL_1 1 //   4
#define AFS_SEL_2 2 //  8
#define AFS_SEL_3 3 //  16

#define MOUSE_L_PIN 5
#define MOUSE_R_PIN 6
#define MOUSE_SCROLL_PIN A0

// TODO: SCALE DOES NOT WORK

typedef struct {
  float x;
  float y;
  float z;
} Vector3;

const float SCALE_FACTORS[] = {131, 65.5, 32.8, 16.4};
const float A_SCALE_FACTORS[] = { 16384, 8192, 4096, 2048};

int smoothing = 10;
int smoothingCount = 0;
Vector3 zero = {0, 0, 0};
Vector3 smoothingCache[] = {zero, zero, zero, zero, zero, zero, zero, zero, zero, zero};

int FS_SEL = FS_SEL_3;
int AFS_SEL = AFS_SEL_1;

float LSB_PER_DEG_PER_SEC = SCALE_FACTORS[FS_SEL];
float LSB_PER_G = A_SCALE_FACTORS[AFS_SEL];

int calibrateIterations = 100;
Vector3 gyroError = zero;
Vector3 accelError = zero;

bool gotAcknowledge = false;
bool timerFlag = false;
unsigned long diff = 0;
unsigned long prevMicros = 0;
int prevScroll = 0;
int currScroll;
int deltaScroll;

void setup() {
  pinMode(MOUSE_L_PIN, INPUT);
  pinMode(MOUSE_R_PIN, INPUT);
  pinMode(MOUSE_SCROLL_PIN, INPUT);

  Serial.begin(115200); // for bluetooth module HC-05
  resetMPU6050();
  delay(1);
  calibrateGyro();
  delay(1);
  calibrateAccel();
  delay(1);

  //  Serial.print("AT\r\n");
  //  delay(100);
  //  Serial.print("AT+VERSION?\r\n");
  //  delay(100);
  //  Serial.print("AT+NAME?\r\n");
  //  delay(100);
  //  Serial.print("AT+UART?\r\n");
  //  delay(100);
  //  Serial.print("AT+PSWD?\r\n");

  cli(); // stop interrupts

  // set timer1 interrupt at 1Hz
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
  unsigned long currMicros = micros();
  diff = currMicros - prevMicros;
  prevMicros = currMicros;
  timerFlag = true;
}
void loop() {
  //  while (Serial.available() > 0) {
  //    Serial.print((char) Serial.read());
  //  }

  while (Serial.available() > 0) {
    if (Serial.read() == 'A') {
      Serial.print('A');
      gotAcknowledge = true;
      while (Serial.available() > 0) { // empty in buffer
        Serial.read();
      }
      delay(100);
    }
  }

  if (!gotAcknowledge) return;

  smoothingCache[smoothingCount] = readGyro();
  smoothingCount = (smoothingCount + 1) % smoothing;

  if (timerFlag) {
    sendData();
    timerFlag = false;
  }

  //delayMicroseconds(100);

  //  Vector3 accels = readAccel();
  //  float ang = atan2(accels.x, accels.z) * 180 / PI; // y/x
  //  Serial.println(ang);
  //  delay(10);
}

void sendData() {
  Vector3 gyroSum = zero;
  for (int i = 0; i < smoothing; i++) {
    gyroSum = add(gyroSum, smoothingCache[i]);
  }
  gyroSum = multiply(gyroSum, 1.0 / smoothing);
  byte* buf = (byte*) &gyroSum;
  Serial.write(buf, sizeof(Vector3));
  byte buttonData = ((digitalRead(MOUSE_L_PIN) == HIGH) << 1) + (digitalRead(MOUSE_R_PIN) == HIGH);
  Serial.write(buttonData);

  currScroll = analogRead(MOUSE_SCROLL_PIN);

  bool isScrollPressed = currScroll > 30;

  if (isScrollPressed) {
    if (prevScroll == -1) {
      prevScroll = currScroll;
    }
    deltaScroll = currScroll - prevScroll;
    prevScroll = currScroll;
  } else {
    prevScroll = -1;
  }

  if (deltaScroll > 127) deltaScroll = 127;
  if (deltaScroll < -128) deltaScroll = -128;
  
  byte scrollData = deltaScroll;
  scrollData &= 0b11111110; // reset bit 0
  scrollData |= isScrollPressed; // set bit 0 to pressed
  Serial.write(scrollData);
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
  for (int i = 0; i < calibrateIterations; ++i) {
    errorSum = add(errorSum, readGyro());
    delay(1);
  }
  gyroError = multiply(errorSum, -1.0 / calibrateIterations);
}

Vector3 calibrateAccel() {
  Vector3 errorSum = zero;
  for (int i = 0; i < calibrateIterations; ++i) {
    errorSum = add(errorSum, readAccel());
    delay(1);
  }
  errorSum = add(errorSum, { 0, 0, -calibrateIterations} ); // subtract off gravity (1g)
  accelError = multiply(errorSum, -1.0 / calibrateIterations);
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
