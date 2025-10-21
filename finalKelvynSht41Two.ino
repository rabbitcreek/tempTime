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
int successfulTransmissions = 0;  // Counter for successful BLE transmissions
const int REQUIRED_TRANSMISSIONS = 2;  // Number of transmissions before sleep
unsigned long connectionTimeout = 30000;  // 30 seconds to wait for connection
unsigned long startAdvertising = 0;  // Track when advertising started
float lastTemperatureF = 0;  // Store last temperature for sleep display

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
    successfulTransmissions = 0;  // Reset counter on new connection
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
const int oneWireBus = D7;  // Moved from D7 to free up D7 for LED
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// =======================
// LED Setup
// =======================
const int LED_RIGHT = D8;  // Red LED - tens place indicator
const int LED_LEFT = D9;   // LED - ones place indicator

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
// LED Flash Function
// =======================
void flashLED(int ledPin, int numFlashes, int flashDuration = 1000) {
  for (int i = 0; i < numFlashes; i++) {
    digitalWrite(ledPin, HIGH);
    delay(flashDuration);
    digitalWrite(ledPin, LOW);
    delay(500);  // Short pause between flashes
  }
}

// =======================
// Display Temperature on LEDs
// =======================
void displayTemperatureOnLEDs(float temperatureF) {
  int temp = (int)temperatureF;  // Get integer part
  int tens = temp / 10;          // Tens place (e.g., 7 for 75°F)
  int ones = temp % 10;          // Ones place (e.g., 5 for 75°F)
  
  Serial.print("Displaying temperature on LEDs: ");
  Serial.print(temp);
  Serial.print("°F (");
  Serial.print(tens);
  Serial.print(" tens, ");
  Serial.print(ones);
  Serial.println(" ones)");
  
  // Flash D8 (RIGHT/RED) for tens place
  if (tens > 0) {
    Serial.print("Flashing RIGHT LED (D8) ");
    Serial.print(tens);
    Serial.println(" times for tens place");
    flashLED(LED_RIGHT, tens, 1000);
    delay(1000);  // Pause between tens and ones
  }
  
  // Flash D7 (LEFT) for ones place
  if (ones > 0) {
    Serial.print("Flashing LEFT LED (D7) ");
    Serial.print(ones);
    Serial.println(" times for ones place");
    flashLED(LED_LEFT, ones, 1000);
  }
  
  // If temperature is exactly divisible by 10 (no ones), still show zeros
  if (ones == 0 && tens > 0) {
    Serial.println("No ones place to display (0)");
  }
  
  delay(1000);  // Final pause before powering down
}

// =======================
// Setup
// =======================
void setup() {
  Serial.begin(115200);
  pinMode(oneWireBus, INPUT);
  pinMode(D10, OUTPUT);
  digitalWrite(D10, HIGH);
  
  // Setup LED pins
  pinMode(LED_RIGHT, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  digitalWrite(LED_RIGHT, LOW);
  digitalWrite(LED_LEFT, LOW);
  
  Serial.println("ESP32 waking up...");

  // Initialize temperature sensor
  sensors.begin();
  
  // Read initial temperature and display on LEDs
  Serial.println("\n=== Reading initial temperature ===");
  sensors.requestTemperatures();
  float initialTempC = sensors.getTempCByIndex(0);
  float initialTempF = sensors.getTempFByIndex(0);
  
  // Validate temperature reading
  if (initialTempC > -127 && initialTempC < 125) {  // Valid DS18B20 range
    Serial.print("Initial Temperature: ");
    Serial.print(initialTempC);
    Serial.print("°C (");
    Serial.print(initialTempF);
    Serial.println("°F)");
    
    // Display temperature on LEDs
    Serial.println("=== Displaying initial temperature on LEDs ===");
    displayTemperatureOnLEDs(initialTempF);
    Serial.println("=== LED display complete ===\n");
  } else {
    Serial.println("Invalid temperature reading, skipping LED display");
  }
  
  // Store the temperature for later use if needed
  lastTemperatureF = initialTempF;
  
  // Initialize BLE after LED display
  InitBLE();

  startAdvertising = millis();  // Start advertising timer
  successfulTransmissions = 0;  // Reset transmission counter
  Serial.println("Kelvyn Temperature Monitor started. Waiting for iPhone connection...");
  Serial.print("Will timeout after ");
  Serial.print(connectionTimeout / 1000);
  Serial.println(" seconds if no connection");
}

// =======================
// Go to sleep function
// =======================
void goToSleep(float temperatureF) {
  Serial.println("\n=== Preparing for deep sleep ===");
  
  // Disconnect client if still connected
  if (_BLEClientConnected) {
    Serial.println("Disconnecting client before sleep...");
    BLEDevice::getServer()->disconnect(BLEDevice::getServer()->getConnId());
    delay(500);  // Give time for disconnect to complete
  }
  
  // Clean up BLE
  BLEDevice::deinit();
  
  // Power down LEDs
  digitalWrite(LED_RIGHT, LOW);
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(D10, LOW);
  delay(100);

  // Sleep for timeOut duration
  uint64_t sleepTimeUs = (uint64_t)timeOut * 60ULL * 1000000ULL;
  esp_sleep_enable_timer_wakeup(sleepTimeUs);

  Serial.print("Entering deep sleep for ");
  Serial.print(timeOut);
  Serial.println(" minute(s)...");
  Serial.flush();
  esp_deep_sleep_start();
}

// =======================
// Main loop
// =======================
void loop() {
  // Check if we've been waiting too long for a connection
  if (!_BLEClientConnected && (millis() - startAdvertising > connectionTimeout)) {
    Serial.println("\n*** Connection timeout - No iPhone found ***");
    Serial.print("Waited ");
    Serial.print((millis() - startAdvertising) / 1000);
    Serial.println(" seconds");
    Serial.println("Going to sleep to save battery...");
    delay(1000);
    goToSleep(lastTemperatureF);
  }
  
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);

  // Validate temperature reading
  if (temperatureC < -127 || temperatureC > 125) {
    Serial.println("Invalid temperature reading, skipping this cycle");
    delay(5000);
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.print("°C (");
  Serial.print(temperatureF);
  Serial.println("°F)");
  
  // Update stored temperature
  lastTemperatureF = temperatureF;

  heart[1] = (byte)temperatureF;  // send integer part of temp as "heart rate"
  heartRateMeasurementCharacteristics.setValue(heart, 8);
  
  if (_BLEClientConnected) {
    heartRateMeasurementCharacteristics.notify();
    successfulTransmissions++;
    
    Serial.print("Temperature sent as Heart Rate Monitor data! (Transmission ");
    Serial.print(successfulTransmissions);
    Serial.print("/");
    Serial.print(REQUIRED_TRANSMISSIONS);
    Serial.println(")");
    
    // Check if we've completed required transmissions
    if (successfulTransmissions >= REQUIRED_TRANSMISSIONS) {
      Serial.println("\n*** Required transmissions complete! ***");
      delay(1000);  // Brief delay before sleep
      goToSleep(temperatureF);
    }
  } else {
    // Show how long we've been waiting
    unsigned long waitTime = (millis() - startAdvertising) / 1000;
    Serial.print("No iPhone connected - waiting... (");
    Serial.print(waitTime);
    Serial.print("/");
    Serial.print(connectionTimeout / 1000);
    Serial.println(" seconds)");
  }

  delay(5000);  // sensor update interval
}
