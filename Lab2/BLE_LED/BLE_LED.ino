/*
  LED

  This example creates a Bluetooth® Low Energy peripheral with service that contains a
  characteristic to control an LED.

  The circuit:
  - Arduino MKR WiFi 1010, Arduino Uno WiFi Rev2 board, Arduino Nano 33 IoT,
    Arduino Nano 33 BLE, or Arduino Nano 33 BLE Sense board.

  You can use a generic Bluetooth® Low Energy central app, like LightBlue (iOS and Android) or
  nRF Connect (Android), to interact with the services and characteristics
  created in this sketch.

  This example code is in the public domain.
*/

#include <ArduinoBLE.h>  // BLE

#include "LSM6DS3.h"  // IMU
#include "Wire.h"     // IMU & OLED

#include <PCF8563.h>  // RTC

#include <Arduino.h>  // OLED
#include <U8x8lib.h>  // OLED - Display library

#include <SPI.h>  // SD Card
#include <SD.h>   // SD Card

// Prototypes
void setupLED();
void setupBLE();
void setupIMU();
void setupRTC();
void setupOLED();
void setupSDCard();

void printIMU();
void printRTC();
void displayOnOLED(const char str[], const uint32_t sampling_freq);
void writeToSDCard();

/* Global Vars */
/* BLE Globals */
BLEService ledService("19B10000-E8F2-537E-4F6C-D104768A1214");  // Bluetooth® Low Energy LED Service

// Bluetooth® Low Energy LED Switch Characteristic - custom 128-bit UUID, read and writable by central
BLEByteCharacteristic switchCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite);

/* LED Global */
const int ledPin = LED_BUILTIN;  // pin to use for the LED

/* IMU Global */
//Create a instance of class LSM6DS3
LSM6DS3 myIMU(I2C_MODE, 0x6A);  //I2C device address 0x6A

/* RTC Global */
PCF8563 pcf;

/* OLED Global */
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* clock=*/PIN_WIRE_SCL, /* data=*/PIN_WIRE_SDA, /* reset=*/U8X8_PIN_NONE);  // OLEDs without Reset of the Display

/* SD Card Global */
const int chipSelect = 2;

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  setupLED();
  setupBLE();
  setupIMU();
  setupRTC();
  setupOLED();
  setupSDCard();
}


void setupLED() {
  // set LED pin to output mode
  pinMode(ledPin, OUTPUT);
}

void setupBLE() {

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");

    while (1)
      ;
  }

  // set advertised local name and service UUID:
  BLE.setLocalName("LED-AG");
  BLE.setAdvertisedService(ledService);

  // add the characteristic to the service
  ledService.addCharacteristic(switchCharacteristic);

  // add service
  BLE.addService(ledService);

  // set the initial value for the characteristic:
  switchCharacteristic.writeValue(0);

  // start advertising
  BLE.advertise();

  Serial.println("BLE LED Peripheral");
}

void setupIMU() {
  //Call .begin() to configure the IMUs
  if (myIMU.begin() != 0) {
    Serial.println("Device error");
  } else {
    Serial.println("Device OK!");
  }
}

void setupRTC() {
  pcf.init();  //initialize the clock

  pcf.stopClock();  //stop the clock

  pcf.startClock();  //start the clock
}

void setupOLED() {
  u8x8.begin();
  u8x8.setFlipMode(1);  // set number from 1 to 3, the screen word will rotary 180
}

void setupSDCard() {
  Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("1. is a card inserted?");
    Serial.println("2. is your wiring correct?");
    Serial.println("3. did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    while (true)
      ;
  }

  Serial.println("initialization done.");
}

/* Main BLE Loop */
void loop() {
  // listen for Bluetooth® Low Energy peripherals to connect:
  BLEDevice central = BLE.central();

  // Mode we are in
  uint8_t mode = 1;

  // Sampling Freq
  uint32_t sampling_freq = 1000;

  // if a central is connected to peripheral:
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's MAC address:
    Serial.println(central.address());

    // while the central is still connected to peripheral:
    while (central.connected()) {

      // if the remote device wrote to the characteristic,
      // use the value to control the LED:
      if (switchCharacteristic.written()) {

        if (switchCharacteristic.value()) {  // any value other than 0
          digitalWrite(ledPin, LOW);         // will turn the LED on

          displayOnOLED("MODE -", sampling_freq);

          mode = 1;
        } else {                       // a 0 value
          digitalWrite(ledPin, HIGH);  // will turn the LED off

          displayOnOLED("MODE", sampling_freq);

          mode = 0;
        }

        Serial.println();
        Serial.println();
      }

      printIMU();
      printRTC();

      if (mode == 0) {
        Serial.println("Writing to SD Card");
        writeToSDCard();
      }

      delay(sampling_freq);
    }

    // when the central disconnects, print it out:
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
  }
}

/* Prints IMU values (Accelerometer, Gyroscope, and Thermometer) to terminal*/
void printIMU() {
  //Accelerometer
  Serial.print("\nAccelerometer:\n");
  Serial.print(" X1 = ");
  Serial.println(myIMU.readFloatAccelX(), 4);
  Serial.print(" Y1 = ");
  Serial.println(myIMU.readFloatAccelY(), 4);
  Serial.print(" Z1 = ");
  Serial.println(myIMU.readFloatAccelZ(), 4);

  //Gyroscope
  Serial.print("\nGyroscope:\n");
  Serial.print(" X1 = ");
  Serial.println(myIMU.readFloatGyroX(), 4);
  Serial.print(" Y1 = ");
  Serial.println(myIMU.readFloatGyroY(), 4);
  Serial.print(" Z1 = ");
  Serial.println(myIMU.readFloatGyroZ(), 4);

  //Thermometer
  Serial.print("\nThermometer:\n");
  Serial.print(" Degrees C1 = ");
  Serial.println(myIMU.readTempC(), 4);
  Serial.print(" Degrees F1 = ");
  Serial.println(myIMU.readTempF(), 4);
}

/* Prints RTC */
void printRTC() {
  Time nowTime = pcf.getTime();  //get current time

  //print current time
  Serial.print(nowTime.day);
  Serial.print("/");
  Serial.print(nowTime.month);
  Serial.print("/");
  Serial.print(nowTime.year);
  Serial.print(" ");
  Serial.print(nowTime.hour);
  Serial.print(":");
  Serial.print(nowTime.minute);
  Serial.print(":");
  Serial.println(nowTime.second);
}

/* Displays str to screen */
void displayOnOLED(const char str[], const uint32_t sampling_freq) {
  String sample_freq_str = "Rate(ms): ";
  sample_freq_str += sampling_freq;
  
  u8x8.setFont(u8x8_font_chroma48medium8_r);  //try u8x8_font_px437wyse700a_2x2_r
  u8x8.setCursor(0, 0);                       // It will start printing from (0,0) location
  u8x8.print(str);
  u8x8.setCursor(0, 1);                       // It will start printing from (0,0) location
  u8x8.print(sample_freq_str);
}

/* Write data to SD Card*/
void writeToSDCard(void) {
  Time nowTime = pcf.getTime();  //get current time
  // make a string for assembling the data to log:
  String dataString = "";

  dataString += nowTime.hour;
  dataString += ",";
  dataString += nowTime.minute;
  dataString += ",";
  dataString += nowTime.second;
  dataString += ",";
  dataString += myIMU.readFloatAccelX();
  dataString += ",";
  dataString += myIMU.readFloatAccelY();
  dataString += ",";
  dataString += myIMU.readFloatAccelZ();
  dataString += ",";
  dataString += myIMU.readFloatGyroX();
  dataString += ",";
  dataString += myIMU.readFloatGyroY();
  dataString += ",";
  dataString += myIMU.readFloatGyroZ();
  dataString += ",";
  dataString += myIMU.readTempF();

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}
