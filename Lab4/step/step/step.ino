/*
  BLE Step Counter (100-sample batches)

  - BLE advertises a custom Step Service with a Step Count characteristic (uint32, Read+Notify).
  - While connected, it samples 100 accel samples, computes magnitude, counts steps using a simple
    threshold + hysteresis + debounce, and notifies updated step count.

  View on phone:
  - nRF Connect / LightBlue: connect -> find "Step Service" -> subscribe to Step Count notifications.
*/

#include <ArduinoBLE.h>
#include "LSM6DS3.h"
#include "Wire.h"

// -------------------- IMU --------------------
LSM6DS3 myIMU(I2C_MODE, 0x6A);  // I2C addr 0x6A (matches your code)

// -------------------- BLE --------------------
// Custom Step Service + Step Count Characteristic (uint32)
BLEService stepService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEUnsignedIntCharacteristic stepCountChar(
  "19B10002-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLENotify
);

// -------------------- Step counting params --------------------
static const int   SAMPLES_PER_BATCH   = 100;
static const int   SAMPLE_INTERVAL_MS  = 20;   // 50 Hz -> 100 samples ~ 2 seconds

// Simple high-pass via EMA baseline on magnitude
static const float EMA_ALPHA           = 0.05f;  // baseline smoothing (0..1)

// Thresholds on (mag - baseline) in "g" units (tune these)
static const float STEP_HIGH_THRESH_G  = 0.18f;
static const float STEP_LOW_THRESH_G   = 0.10f;

// Debounce to avoid double-counting
static const unsigned long MIN_STEP_INTERVAL_MS = 250;

// -------------------- State --------------------
uint32_t stepCount        = 0;
uint32_t oldStepCount     = 0;

float    emaMagBaseline   = 1.0f;   // start near 1g
bool     inStep           = false;  // hysteresis state
unsigned long lastStepMs  = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  pinMode(LEDR, OUTPUT);

  // IMU init
  Wire.begin();
  if (myIMU.begin() != 0) {
    Serial.println("IMU Device error");
    while (1) {}
  }
  Serial.println("IMU Device OK");

  // BLE init (BatteryMonitor-style setup)
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1) {}
  }

  BLE.setLocalName("StepCounter-AG");
  BLE.setAdvertisedService(stepService);

  stepService.addCharacteristic(stepCountChar);
  BLE.addService(stepService);

  stepCountChar.writeValue(stepCount);  // initial value

  BLE.advertise();
  Serial.println("BLE device active, waiting for connections...");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    digitalWrite(LEDR, HIGH);

    while (central.connected()) {
      sample100AndUpdateSteps(central);

      // Notify only if changed (like BatteryMonitor example)
      if (stepCount != oldStepCount) {
        stepCountChar.writeValue(stepCount);
        oldStepCount = stepCount;

        Serial.print("Steps: ");
        Serial.println(stepCount);
      }

      BLE.poll(); // keep BLE stack happy during long loops
    }

    digitalWrite(LEDR, LOW);
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}

void sample100AndUpdateSteps(BLEDevice &central) {
  for (int i = 0; i < SAMPLES_PER_BATCH; i++) {
    if (!central.connected()) return;

    float ax = myIMU.readFloatAccelX();
    float ay = myIMU.readFloatAccelY();
    float az = myIMU.readFloatAccelZ();

    // Magnitude in g
    float mag = sqrtf(ax * ax + ay * ay + az * az);

    // EMA baseline (gravity-ish)
    emaMagBaseline = (1.0f - EMA_ALPHA) * emaMagBaseline + EMA_ALPHA * mag;

    // “Dynamic” magnitude (high-pass)
    float dyn = mag - emaMagBaseline;

    // Step detection: threshold + hysteresis + debounce
    unsigned long now = millis();

    if (!inStep) {
      if (dyn > STEP_HIGH_THRESH_G && (now - lastStepMs) >= MIN_STEP_INTERVAL_MS) {
        stepCount++;
        lastStepMs = now;
        inStep = true;
      }
    } else {
      if (dyn < STEP_LOW_THRESH_G) {
        inStep = false;
      }
    }

    delay(SAMPLE_INTERVAL_MS);
    BLE.poll();
  }
}
