#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <NimBLEDevice.h>
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"

// SHT40 sensor
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// BLE service and characteristics UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define TEMP_CHAR_UUID      "12345678-1234-1234-1234-1234567890ac"

// Sleep configuration
#define SLEEP_DURATION_US   60000000  // 60 seconds in microseconds

NimBLEServer* pServer;
NimBLECharacteristic* pTempCharacteristic;

// Connection tracking (simplified)
bool clientConnected = false;

// Function declarations
void goToSleep();

// BLE Server Callbacks - Define class before using it
class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("iPhone connected!");
        clientConnected = true;
    }

    void onDisconnect(NimBLEServer* pServer) {
        Serial.println("iPhone disconnected!");
        clientConnected = false;
        
        // Restart advertising
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->start();
    }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Xiao ESP32C6 SHT40 + BLE with Sleep Mode");

  // Init I2C sensor
  if (!sht4.begin()) {
    Serial.println("Couldn't find SHT40 sensor");
    while (1) delay(10);
  }
  Serial.println("SHT40 sensor initialized");

  // BLE init
  NimBLEDevice::init("XiaoThermo");  // device name seen by iPhone
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pTempCharacteristic = pService->createCharacteristic(
                          TEMP_CHAR_UUID,
                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                        );

  pService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponseData(NimBLEAdvertisementData());
  pAdvertising->start();

  Serial.println("BLE advertising started...");
}

void loop() {
  // Take a temperature reading
  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp); // populate temp and humidity

  float temperatureC = temp.temperature;
  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.print(" °C, Humidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println(" %");

  // Wait for iPhone to connect and send data
  bool dataSent = false;
  unsigned long startTime = millis();
  unsigned long maxWaitTime = 30000; // Wait up to 30 seconds for connection
  
  while (!dataSent && (millis() - startTime < maxWaitTime)) {
    bool hasConnectedClient = (pServer->getConnectedCount() > 0);
    
    if (hasConnectedClient) {
      Serial.println("iPhone connected - sending data");
      
      // Update BLE characteristic
      char tempStr[16];
      dtostrf(temperatureC, 4, 2, tempStr);
      strcat(tempStr, ",");
      dtostrf(humidity.relative_humidity, 4, 2, tempStr + strlen(tempStr));
      
      pTempCharacteristic->setValue(tempStr);
      pTempCharacteristic->notify();
      
      // Give time for data to be sent
      delay(2000);
      dataSent = true;
      Serial.println("Data sent successfully!");
    } else {
      Serial.println("Waiting for iPhone connection...");
      delay(2000);
    }
  }
  
  if (!dataSent) {
    Serial.println("Timeout - no iPhone connection, going to sleep anyway");
  }

  // Go to sleep after sending data (or timeout)
  Serial.println("Going to sleep for 60 seconds...");
  
  // Simple delay instead of deep sleep for testing
   // 60 seconds delay
  
  // Uncomment the line below to use deep sleep instead of delay:
   goToSleep();
}

void goToSleep() {
  Serial.println("Preparing for sleep...");
  delay(1000); // Give time for serial output
  
  Serial.println("Going to sleep for 60 seconds...");
  Serial.flush();
  
  // Configure wake sources - timer wakeup only
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
  delay(60000);
  // Enter deep sleep - ESP32 will handle cleanup automatically
  esp_deep_sleep_start();
}

