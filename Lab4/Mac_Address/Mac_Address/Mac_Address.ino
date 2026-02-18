#include <Arduino.h>
#include <ArduinoBLE.h>

void setup() {
  Serial.begin(115200);
  delay(2000);  // give USB time
  Serial.println("BOOT");

  // Start BLE
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1) { delay(10); }
  }

  // Print LOCAL BLE address (MAC-like)
  Serial.print("BLE Address: ");
  Serial.println(BLE.address());

  // (Optional) advertise so you can also see it in scanning apps
  BLE.setLocalName("Data Collection");
  BLE.advertise();

  Serial.println("Advertising...");
}

void loop() {
  // nothing
}