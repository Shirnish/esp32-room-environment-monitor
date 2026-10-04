#include "DHTesp.h"
DHTesp dht;
// DHT11 DATA connects to GPIO 21, with a 10 kOhm pull-up to 3.3 V.
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#define SDA 14
#define SCL 13
LiquidCrystal_I2C lcd(0x27, 16, 2);
const int dhtPin = 21;
unsigned long lastReadTime = 0;
const unsigned long readInterval = 2000;
void setup() {
  dht.setup(dhtPin, DHTesp::DHT11);
  Wire.begin(SDA, SCL);
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();
  Serial.begin(115200);
  Serial.println("Starting up");
}

bool i2CAddrTest(uint8_t addr) {
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  return false;
}

void updateScreen(float temperatureC, float humidity, float temperatureF) {
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(temperatureC, 1);
  lcd.print("C/");
  lcd.print(temperatureF, 1);
  lcd.print("F");
  lcd.setCursor(0, 1);
  lcd.print("Humidity:");
  lcd.print(humidity, 1);
  lcd.print("%");
}

void sensorError() {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print("Sensor error");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("Check Wiring");
}

void readEnvironment() {
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Only convert and print measurements when the sensor read succeeds.
  if (dht.getStatus() != 0) {
    Serial.println(dht.getStatusString());
    sensorError();
  } else {
    float fahrenheit;
    fahrenheit = (newValues.temperature * 1.8) + 32;
    Serial.println(" Temperature:" + String(newValues.temperature) + "°C" + " Temperature Fahrenheit:" + String(fahrenheit) + "°F" + " Humidity:" + String(newValues.humidity) + "%");
    updateScreen(newValues.temperature, newValues.humidity, fahrenheit);
  }
}


void loop() {
  if (millis() - lastReadTime >= readInterval) {
    // Schedule from each attempt, including failures, to avoid rapid retries.
    lastReadTime = millis();
    readEnvironment();
  }
}
