#include <Wire.h>
#include <Adafruit_SHT4x.h>

Adafruit_SHT4x sht40;

void setup() {
  Serial.begin(9600);
  pinMode(D10,OUTPUT);
  digitalWrite(D10,HIGH);
  if (!sht40.begin()) {
    Serial.println("Couldn't find SHT40");
    while (1) delay(1);
  }
}

void loop() {
  sensors_event_t humidity, temp;
  sht40.getEvent(&humidity, &temp); // Get new data

  Serial.print("Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" degrees C");
Serial.print("Temperature: "); Serial.print(temp.temperature * 1.8 + 32); Serial.println(" degrees F");
  Serial.print("Humidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println("% rH");

  delay(1000);
}