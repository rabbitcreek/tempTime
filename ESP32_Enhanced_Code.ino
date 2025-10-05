#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <NimBLEDevice.h>
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_bt.h"

// SHT40 sensor
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// BLE service and characteristics UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define TEMP_CHAR_UUID      "12345678-1234-1234-1234-1234567890ac"

// Sleep configuration
#define SLEEP_DURATION_US   60000000  // 60 seconds in microseconds

NimBLEServer* pServer;
NimBLECharacteristic* pTempCharacteristic;

// Connection tracking
bool clientConnected = false;
unsigned long lastWakeTime = 0;
unsigned long connectionTimeout = 30000; // 30 seconds connection timeout

// BLE Server Callbacks - Define class before using it
class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("Client connected");
        clientConnected = true;
        lastWakeTime = millis(); // Reset wake time when client connects
    }

    void onDisconnect(NimBLEServer* pServer) {
        Serial.println("Client disconnected");
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
  lastWakeTime = millis();
}

void loop() {
  // Check if we should go to sleep
  if (!clientConnected && (millis() - lastWakeTime > connectionTimeout)) {
    Serial.println("No client connected, going to sleep...");
    goToSleep();
    return;
  }

  // Only take readings if connected
  if (clientConnected) {
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp); // populate temp and humidity

    float temperatureC = temp.temperature;
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.print(" °C, Humidity: ");
    Serial.print(humidity.relative_humidity);
    Serial.println(" %");

    // Update BLE characteristic
    char tempStr[16];
    dtostrf(temperatureC, 4, 2, tempStr);
    strcat(tempStr, ",");
    dtostrf(humidity.relative_humidity, 4, 2, tempStr + strlen(tempStr));
    
    pTempCharacteristic->setValue(tempStr);
    pTempCharacteristic->notify();

    // Wait a bit before next reading
    delay(5000);
  } else {
    // Not connected, check for connections
    delay(100);
  }
}

void goToSleep() {
  Serial.println("Preparing for sleep...");
  
  // Stop BLE advertising
  NimBLEDevice::stopAdvertising();
  
  // Disconnect any connected clients
  if (pServer) {
    pServer->disconnect(0);
  }
  
  // Deinitialize BLE
  NimBLEDevice::deinit(true);
  
  // Disable WiFi and Bluetooth radio
  esp_wifi_stop();
  esp_bt_controller_disable();
  
  Serial.println("Going to sleep for 60 seconds...");
  Serial.flush();
  
  // Configure wake sources
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
  // Note: External wakeup removed for compatibility
  
  // Enter deep sleep
  esp_deep_sleep_start();
}

