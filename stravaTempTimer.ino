#include <OneWire.h>
#include <DallasTemperature.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "esp_sleep.h"

// =======================
// Adjustable sleep timing
// =======================
int timeOut = 1;  // minutes (set between 1 and 10)
int timeWake = 1;
unsigned long lastActive = 0;  // track active time
unsigned long sleepMillis = (unsigned long)timeWake * 60UL * 1000UL;

// =======================
// BLE & Sensor Setup
// =======================
byte flags = 0b00111110;
byte bpm;
byte heart[8] = { 0b00001110, 60, 0, 0, 0 , 0, 0, 0 };
byte hrmPos[1] = { 2 };

bool _BLEClientConnected = false;

#define heartRateService BLEUUID((uint16_t)0x180D)
BLECharacteristic heartRateMeasurementCharacteristics(
  BLEUUID((uint16_t)0x2A37),
  BLECharacteristic::PROPERTY_NOTIFY
);
BLECharacteristic sensorPositionCharacteristic(
  BLEUUID((uint16_t)0x2A38),
  BLECharacteristic::PROPERTY_READ
);
BLEDescriptor heartRateDescriptor(BLEUUID((uint16_t)0x2901));
BLEDescriptor sensorPositionDescriptor(BLEUUID((uint16_t)0x2901));

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    _BLEClientConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    _BLEClientConnected = false;
  }
};

// =======================
// OneWire temperature setup
// =======================
const int oneWireBus = D7;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// =======================
// BLE initialization
// =======================
void InitBLE() {
  BLEDevice::init("FT7");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pHeart = pServer->createService(heartRateService);
  pHeart->addCharacteristic(&heartRateMeasurementCharacteristics);
  heartRateDescriptor.setValue("Rate from 0 to 200");
  heartRateMeasurementCharacteristics.addDescriptor(&heartRateDescriptor);
  heartRateMeasurementCharacteristics.addDescriptor(new BLE2902());

  pHeart->addCharacteristic(&sensorPositionCharacteristic);
  sensorPositionDescriptor.setValue("Position 0 - 6");
  sensorPositionCharacteristic.addDescriptor(&sensorPositionDescriptor);

  pServer->getAdvertising()->addServiceUUID(heartRateService);

  pHeart->start();
  pServer->getAdvertising()->start();
}

// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);
  pinMode(oneWireBus, INPUT);
  pinMode(D10, OUTPUT);
  digitalWrite(D10, HIGH);

  sensors.begin();
  InitBLE();

  lastActive = millis();
  Serial.println("System started. Running until deep sleep timeout...");
}

// =======================
// Main loop
// =======================
void loop() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);

  Serial.print(temperatureC);
  Serial.println("ºC");
  Serial.print(temperatureF);
  Serial.println("ºF");

  heart[1] = (byte)temperatureF;  // send integer part of temp
  heartRateMeasurementCharacteristics.setValue(heart, 8);
  heartRateMeasurementCharacteristics.notify();

  delay(5000);  // sensor update interval

  // =======================
  // Deep sleep timer check
  // =======================
  if (millis() - lastActive >= sleepMillis) {
    Serial.println("Timeout reached. Going to deep sleep...");

    // Clean up before sleeping
    BLEDevice::deinit();
    digitalWrite(D10, LOW);
    delay(100);

    // Sleep for a defined time (e.g., wake up after timeOut minutes)
    uint64_t sleepTimeUs = (uint64_t)timeOut * 60ULL * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleepTimeUs);

    Serial.flush();
    delay(30000);
    esp_deep_sleep_start();
  }
}
