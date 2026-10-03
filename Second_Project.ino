#include "DHTesp.h"
DHTesp dht;
// DHT11 DATA connects to GPIO 21, with a 10 kOhm pull-up to 3.3 V.
#define SDA 14
#define SCL 13
LiquidCrystal_I2C lcd(0x27,16,2);
const int dhtPin = 21;
unsigned long lastReadTime = 0;
const unsigned long readInterval = 2000;
void setup() {
  dht.setup(dhtPin, DHTesp::DHT11);
  Serial.begin(115200);
  Serial.println("Starting up");
}

void readEnvironment() {
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Only convert and print measurements when the sensor read succeeds.
  if (dht.getStatus() != 0) {
    Serial.println(dht.getStatusString());
  } else {
    float fahrenheit;
    fahrenheit = (newValues.temperature * 1.8) + 32;
    Serial.println(" Temperature:" + String(newValues.temperature) + "°C" + " Temperature Fahrenheit:" + String(fahrenheit) + "°F" + " Humidity:" + String(newValues.humidity) + "%");
  }
}
void loop() {
  if (millis() - lastReadTime >= readInterval) {
    // Schedule from each attempt, including failures, to avoid rapid retries.
    lastReadTime = millis();
    readEnvironment();
  }
}
