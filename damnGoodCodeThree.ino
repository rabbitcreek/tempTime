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
    lastActive = millis();  // Start timer when connection happens
    Serial.println("iPhone connected to Temperature Monitor!");
  };

  void onDisconnect(BLEServer* pServer) {
    _BLEClientConnected = false;
    Serial.println("iPhone disconnected from Temperature Monitor!");
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
  BLEDevice::init("KelvynTemp");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pHeart = pServer->createService(heartRateService);

  pHeart->addCharacteristic(&heartRateMeasurementCharacteristics);
  heartRateDescriptor.setValue("Temperature from 0 to 200°F");
  heartRateMeasurementCharacteristics.addDescriptor(&heartRateDescriptor);
  heartRateMeasurementCharacteristics.addDescriptor(new BLE2902());

  pHeart->addCharacteristic(&sensorPositionCharacteristic);
  sensorPositionDescriptor.setValue("Position 0 - 6");
  sensorPositionCharacteristic.addDescriptor(&sensorPositionDescriptor);

  pHeart->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(heartRateService);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  
  BLEDevice::startAdvertising();

  Serial.println("Heart Rate Monitor BLE Advertising started...");
  Serial.println("Device name: KelvynTemp");
  Serial.println("Looking for Heart Rate Monitor service...");
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
  Serial.println("Kelvyn Temperature Monitor started. Running...");
}

// =======================
// Main loop
// =======================
void loop() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);

  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.print("°C (");
  Serial.print(temperatureF);
  Serial.println("°F)");

  heart[1] = (byte)temperatureF;  // send integer part of temp as "heart rate"
  heartRateMeasurementCharacteristics.setValue(heart, 8);
  
  if (_BLEClientConnected) {
    heartRateMeasurementCharacteristics.notify();
    Serial.println("Temperature sent as Heart Rate Monitor data!");
  } else {
    Serial.println("No iPhone connected - data not sent");
  }

  delay(5000);  // sensor update interval

  // =======================
  // Deep sleep timer check
  // =======================
  // Check if we've been awake/connected for timeWake duration
  unsigned long awakeTime = millis() - lastActive;
  
  if (awakeTime >= sleepMillis) {
    Serial.print("Been awake for ");
    Serial.print(awakeTime / 1000);
    Serial.println(" seconds. Going to deep sleep...");
    
    // Disconnect client if still connected
    if (_BLEClientConnected) {
      Serial.println("Disconnecting client before sleep...");
      BLEDevice::getServer()->disconnect(BLEDevice::getServer()->getConnId());
      delay(500);  // Give time for disconnect to complete
    }
    
    // Clean up before sleeping
    BLEDevice::deinit();
    digitalWrite(D10, LOW);
    delay(100);

    // Sleep for timeOut duration
    uint64_t sleepTimeUs = (uint64_t)timeOut * 60ULL * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleepTimeUs);

    Serial.println("Entering deep sleep...");
    Serial.flush();
    delay(30000);
    esp_deep_sleep_start();
  }
}
