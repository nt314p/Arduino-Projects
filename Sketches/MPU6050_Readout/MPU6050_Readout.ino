#include <Wire.h>

#define MPU 0x68
#define GYRO_X 0x43
#define MPU_PWR_MGMT_1 0x6B
#define MPU_DEVICE_RESET 0b00000001 // bit banging is MSB first

//               +- deg/sec
#define FS_SEL_0 0 //   250
#define FS_SEL_1 1 //   500
#define FS_SEL_2 2 //  1000
#define FS_SEL_3 3 //  2000

// TODO: SCALE DOES NOT WORK

typedef struct {
  float x;
  float y;
  float z;
} Vector3;

const float SCALE_FACTORS[] = {131, 65.5, 32.8, 16.4};

int smoothing = 10;
int smoothingCount = 0;
Vector3 zero = {0, 0, 0};
Vector3 smoothingCache[] = {zero, zero, zero, zero, zero, zero, zero, zero, zero, zero};

int FS_SEL = FS_SEL_3;
float LSB_PER_DEG_PER_SEC = SCALE_FACTORS[FS_SEL];
Vector3 gyroError = zero;

void setup() {
  Serial.begin(38400); // for bluetooth module HC-05
  resetMPU6050();
  delay(10);
  calibrateGyro();
  delay(10);
}

void loop() {
  smoothingCache[smoothingCount] = readGyro();
  smoothingCount = (smoothingCount + 1) % smoothing;
  
  if (smoothingCount == 0) {
    Vector3 gyroSum = zero;
    for (int i = 0; i < smoothing; i++) {
      gyroSum = add(gyroSum, smoothingCache[i]);
    }
    gyroSum = multiply(gyroSum, 1.0 / smoothing);
    Serial.print(gyroSum.x);
    Serial.print("\t");
    Serial.print(gyroSum.y);
    Serial.print("\t");
    Serial.println(gyroSum.z);
  }
  
  delay(1);
}

void resetMPU6050() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(MPU_PWR_MGMT_1); // select the power management register
  Wire.write(MPU_DEVICE_RESET); // reset device
  Wire.endTransmission(true);
}

Vector3 calibrateGyro() {
  Vector3 errorSum = zero;
  int iterations = 100;
  for (int i = 0; i < iterations; i++) {
    errorSum = add(errorSum, readGyro());
    delay(1);
  }
  gyroError = multiply(errorSum, -1.0 / iterations);
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

Vector3 add(Vector3 a, Vector3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 multiply(Vector3 v, float scalar) {
  return {v.x * scalar, v.y * scalar, v.z * scalar};
}
