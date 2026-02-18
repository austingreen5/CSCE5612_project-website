
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <LSM6DS3.h>
#include <ArduinoBLE.h>

// ---------------- IMU ----------------
LSM6DS3 myIMU(I2C_MODE, 0x6A);  // keep exactly as your working code
float aX, aY, aZ;

// Motion trigger + sampling
const float    accelerationThreshold = 1.0f;   // sum(|ax|+|ay|+|az|) in g
const int      numSamples = 119;
const uint32_t samplePeriodMs = 10;            // 100 Hz

int samplesRead = numSamples;
uint32_t eventCount = 0;

// ---------------- BLE ----------------
// Use your UUIDs (same as ESP32 version)
static const char* SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0";
static const char* CHAR_UUID    = "12345678-1234-5678-1234-56789abcdef1";

BLEService imuService(SERVICE_UUID);

// Notify-only characteristic (add BLERead too if you want debugging)
BLECharacteristic imuChar(CHAR_UUID, BLENotify, 8); 
// 8 bytes payload: ctr(2) + ax(2) + ay(2) + az(2)

// Pack signed 16-bit little-endian
static inline void pack_i16(uint8_t* buf, int idx, int16_t v) {
  buf[idx]     = (uint8_t)(v & 0xFF);
  buf[idx + 1] = (uint8_t)((v >> 8) & 0xFF);
}

static uint16_t ctr = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("BOOT");

  // I2C
  Wire.begin();
  Wire.setClock(400000);

  // IMU init
  int imuStatus = myIMU.begin();
  if (imuStatus != 0) {
    Serial.print("IMU initialization failed, status = ");
    Serial.println(imuStatus);
  } else {
    Serial.println("IMU initialized");
  }

  // BLE init (ArduinoBLE)
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1) { delay(10); }
  }

  BLE.setLocalName("Austin");
  BLE.setDeviceName("Austin");

  // Print local BLE address
  Serial.print("BLE Address: ");
  Serial.println(BLE.address());

  // Setup service + characteristic
  BLE.setAdvertisedService(imuService);
  imuService.addCharacteristic(imuChar);
  BLE.addService(imuService);

  // Start advertising
  BLE.advertise();

  Serial.println("Advertising BLE...");
  Serial.println("Ready. Waiting for motion...");
  Serial.println("Will notify ONLY during motion event (119 samples @100Hz).");
}

void loop() {
  // Required for ArduinoBLE background processing
  BLE.poll();

  // Detect connection state (optional prints)
  BLEDevice central = BLE.central();
  if (central && central.connected()) {
    static bool printed = false;
    if (!printed) {
      Serial.print("Connected to central: ");
      Serial.println(central.address());
      printed = true;
    }
  } else {
    static bool printedDisc = false;
    static String last = "";
    // Reset printed flag when not connected
    // (simple, avoids spam)
  }

  // -------- Wait for motion --------
  while (samplesRead == numSamples) {
    BLE.poll();

    aX = myIMU.readFloatAccelX();
    aY = myIMU.readFloatAccelY();
    aZ = myIMU.readFloatAccelZ();

    float aSum = fabs(aX) + fabs(aY) + fabs(aZ);

    if (aSum >= accelerationThreshold) {
      samplesRead = 0;
      eventCount++;

      Serial.print("\n=== Motion event #");
      Serial.print(eventCount);
      Serial.println(" detected ===");
      break;
    }

    delay(5);
  }

  // -------- Collect + notify samples --------
  while (samplesRead < numSamples) {
    BLE.poll();

    aX = myIMU.readFloatAccelX();
    aY = myIMU.readFloatAccelY();
    aZ = myIMU.readFloatAccelZ();

    // scale into int16 for BLE packet
    int16_t ax_mg = (int16_t)(aX * 1000.0f);
    int16_t ay_mg = (int16_t)(aY * 1000.0f);
    int16_t az_mg = (int16_t)(aZ * 1000.0f);

    uint8_t pkt[8];
    pack_i16(pkt, 0, (int16_t)ctr++);
    pack_i16(pkt, 2, ax_mg);
    pack_i16(pkt, 4, ay_mg);
    pack_i16(pkt, 6, az_mg);

    // Notify ONLY if a central is connected + subscribed
    // (writeValue works even if not subscribed, but no one receives it)
    imuChar.writeValue(pkt, sizeof(pkt));

    samplesRead++;
    delay(samplePeriodMs);

    if (samplesRead == numSamples) {
      Serial.println("Finished logging a motion event.\n");
    }
  }
}