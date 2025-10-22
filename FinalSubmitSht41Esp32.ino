#include <Wire.h>
#include "Adafruit_SHT4x.h"
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

#define temperatureService BLEUUID("12345678-1234-1234-1234-1234567890ab")
BLECharacteristic temperatureCharacteristic(
  BLEUUID("12345678-1234-1234-1234-1234567890ac"),
  BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
);
BLEDescriptor temperatureDescriptor(BLEUUID((uint16_t)0x2901));

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
// SHT41 sensor setup
// =======================
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

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

  BLEService *pTemp = pServer->createService(temperatureService);

  pTemp->addCharacteristic(&temperatureCharacteristic);
  temperatureDescriptor.setValue("Temperature and humidity data");
  temperatureCharacteristic.addDescriptor(&temperatureDescriptor);
  temperatureCharacteristic.addDescriptor(new BLE2902());

  pTemp->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(temperatureService);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  
  BLEDevice::startAdvertising();

  Serial.println("Temperature Monitor BLE Advertising started...");
  Serial.println("Device name: KelvynTemp");
  Serial.println("Using custom temperature service...");
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
// Reset SHT41 Sensor
// =======================
void resetSHT41() {
  Serial.println("Resetting SHT41 sensor...");
  
  // Reset I2C bus
  Wire.end();
  delay(100);
  Wire.begin();
  delay(200);  // Give more time for I2C to stabilize
  
  // Try to reinitialize sensor
  if (sht4.begin()) {
    Serial.println("SHT41 sensor reset successful");
  } else {
    Serial.println("SHT41 sensor reset failed");
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
  pinMode(D10, OUTPUT);
  digitalWrite(D10, HIGH);
  
  // Setup LED pins
  pinMode(LED_RIGHT, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  digitalWrite(LED_RIGHT, LOW);
  digitalWrite(LED_LEFT, LOW);
  
  Serial.println("ESP32 waking up...");
  Serial.println("Initializing I2C...");
  
  // Initialize I2C
  Wire.begin();
  delay(100);  // Give I2C time to stabilize

  // Declare variables at the top to avoid goto issues
  float initialTempC = 21.1;  // Default 70°F in Celsius
  float initialTempF = 70.0;  // Default fallback temperature
  
  // Always reset SHT41 sensor after wake-up (I2C communication issues)
  Serial.println("Resetting SHT41 sensor after wake-up...");
  resetSHT41();
  
  // Read initial temperature and display on LEDs
  Serial.println("\n=== Reading initial temperature ===");
  sensors_event_t humidity, temp;
  
  if (sht4.getEvent(&humidity, &temp)) {
    initialTempC = temp.temperature;
    initialTempF = (initialTempC * 9.0 / 5.0) + 32.0;  // Convert to Fahrenheit
    
    // Validate temperature reading (SHT41 range is typically -40 to +125°C)
    if (initialTempC > -40 && initialTempC < 125) {
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
      Serial.println(" INVALID TEMPERATURE READING - USING FALLBACK");
      initialTempC = 21.1;  // 70°F in Celsius
      initialTempF = 70.0;
      displayTemperatureOnLEDs(initialTempF);
    }
  } else {
    Serial.println("ERROR: Failed to read initial temperature - using fallback");
    initialTempC = 21.1;  // 70°F in Celsius
    initialTempF = 70.0;
    displayTemperatureOnLEDs(initialTempF);
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
  
  // Read temperature from SHT41 sensor
  Serial.println("Attempting to read from SHT41 sensor...");
  sensors_event_t humidity, temp;
  
  // Declare temperature variables
  float temperatureC = 21.1;  // Default 70°F in Celsius
  float temperatureF = 70.0;  // Default fallback temperature
  
  // Read from sensor (should work since we reset in setup)
  if (sht4.getEvent(&humidity, &temp)) {
    temperatureC = temp.temperature;
    temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;  // Convert to Fahrenheit
  } else {
    Serial.println("ERROR: Failed to read from SHT41 sensor!");
    Serial.println("Using last known temperature for transmission");
    temperatureF = lastTemperatureF;
    temperatureC = (temperatureF - 32.0) * 5.0 / 9.0;
  }

  // Validate temperature reading (SHT41 range is typically -40 to +125°C)
  if (temperatureC < -40 || temperatureC > 125) {
    Serial.println("Invalid temperature reading, using fallback");
    temperatureF = 70.0;  // Fallback temperature
    temperatureC = 21.1;  // 70°F in Celsius
  }

  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.print("°C (");
  Serial.print(temperatureF);
  Serial.print("°F), Humidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println("%");
  
  // Update stored temperature
  lastTemperatureF = temperatureF;

  // Validate temperature before sending
  if (temperatureF < 0 || temperatureF > 200) {
    Serial.print("Invalid temperature reading: ");
    Serial.print(temperatureF);
    Serial.println("°F - using fallback");
    temperatureF = 70.0;  // Use fallback temperature
  }
  
  // Debug: Print what we're sending
  Serial.print("Sending temperature as heart rate: ");
  Serial.print((byte)temperatureF);
  Serial.print(" (from ");
  Serial.print(temperatureF);
  Serial.println("°F)");
  
  // Create temperature data string (temperature,humidity)
  String dataString = String(temperatureC, 2) + "," + String(humidity.relative_humidity, 1);
  temperatureCharacteristic.setValue(dataString.c_str());
  
  if (_BLEClientConnected) {
    temperatureCharacteristic.notify();
    successfulTransmissions++;
    
    Serial.print("Temperature and humidity sent as custom data! (Transmission ");
    Serial.print(successfulTransmissions);
    Serial.print("/");
    Serial.print(REQUIRED_TRANSMISSIONS);
    Serial.print(") - ");
    Serial.print(temperatureC, 2);
    Serial.print("°C, ");
    Serial.print(humidity.relative_humidity, 1);
    Serial.println("%");
    
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
