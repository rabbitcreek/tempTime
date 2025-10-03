#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <NimBLEDevice.h>

// SHT40 sensor
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// BLE service and characteristics UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define TEMP_CHAR_UUID      "12345678-1234-1234-1234-1234567890ac"

NimBLEServer* pServer;
NimBLECharacteristic* pTempCharacteristic;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Xiao ESP32C6 SHT40 + BLE baseline test");

  // Init I2C sensor
  if (!sht4.begin()) {
    Serial.println("Couldn't find SHT40 sensor");
    while (1) delay(10);
  }
  Serial.println("SHT40 sensor initialized");

  // BLE init
  NimBLEDevice::init("XiaoThermo");  // device name seen by iPhone
  pServer = NimBLEDevice::createServer();

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pTempCharacteristic = pService->createCharacteristic(
                          TEMP_CHAR_UUID,
                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                        );

  pService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("BLE advertising started...");
}

void loop() {
  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp); // populate temp and humidity

  float temperatureC = temp.temperature;
  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.println(" °C");

  // Update BLE characteristic
  char tempStr[8];
  dtostrf(temperatureC, 4, 2, tempStr);
  pTempCharacteristic->setValue(tempStr);
  pTempCharacteristic->notify();

  delay(10000); // update every 10 seconds
}
