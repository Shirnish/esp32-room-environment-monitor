// Wi-Fi
#include <WiFi.h>
#include "secrets.h"
bool wasWifiConnected;

// Web server
#include <WebServer.h>
WebServer server(80);

// Temperature and humidity
#include "DHTesp.h"
// DATA uses GPIO 21, with a 10 kOhm pull-up to 3.3 V.
const int dhtPin = 21;
const unsigned long readInterval = 2000;
DHTesp dht;
unsigned long lastReadTime = 0;
// Save the latest readings so opening the page doesn't trigger another sensor read.
float latestTemperatureC = 0;
float latestTemperatureF = 0;
float latestHumidity = 0;
bool environmentValid = false;

// Environmental limits. Both endpoints count as within range.
const float minimumTempC = 20;
const float maxTempC = 26;
const float minimumHumidity = 30;
const float maxHumidity = 60;

// Status LEDs
#define led_PinGreen 4
#define led_PinYellow 5
#define led_PinRed 6

// LCD display
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#define SDA 14
#define SCL 13
LiquidCrystal_I2C lcd(0x27, 16, 2);
// The backlight starts on while the PIR warms up.
bool lcdBacklightOn = true;

// Motion and occupancy
#define pir 7
const unsigned long occupancyTimeout = 15000UL;
const unsigned long pirWarmupInterval = 60000UL;
// Recent motion is an estimate of occupancy, not proof someone is in the room.
bool occupied = false;
unsigned long lastMotionTime = 0;
unsigned long pirStartTime;
// Start at -1 so the first PIR reading after warm-up prints once.
int previousMotionState = -1;

// Keep the reason code so I can debug failed Wi-Fi connections.
void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("[WiFi] disconnect reason: %u\n",
                static_cast<unsigned>(info.wifi_sta_disconnected.reason));
}

void setup() {
  pirStartTime = millis();
  pinMode(pir, INPUT);
  pinMode(led_PinYellow, OUTPUT);
  digitalWrite(led_PinYellow, LOW);
  pinMode(led_PinGreen, OUTPUT);
  digitalWrite(led_PinGreen, LOW);
  pinMode(led_PinRed, OUTPUT);
  digitalWrite(led_PinRed, LOW);
  dht.setup(dhtPin, DHTesp::DHT11);
  // SDA and SCL go through the level shifter; the LCD uses 5 V power.
  Wire.begin(SDA, SCL);
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();
  Serial.begin(115200);
  Serial.println("Starting up");
  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  server.on("/", handleRoot);
}

void handleRoot() {
  // Build one response from the saved readings and motion state.
  String info;
  if (!environmentValid) {
    info += "Sensor Reading Unavailable\n";
  } else {
    info += "Temperature: " + String(latestTemperatureC, 1) + "C\n";
    info += "Temperature in F: " + String(latestTemperatureF, 1) + "F\n";
    info += "Humidity: " + String(latestHumidity, 1) + "%\n";
  }
  // Motion status is still available if the DHT reading fails.
  if (millis() - pirStartTime < pirWarmupInterval) {
    info += "PIR warming up\n";
  } else if (occupied) {
    info += "Occupancy: Occupied\n";
  } else {
    info += "Occupancy: No recent Movement\n";
  }
  // The browser reloads every five seconds. Sensor timing stays separate.
  String page = "<!DOCTYPE html><html><head>";
  page += "<meta http-equiv=\"refresh\" content=\"5\">";
  page += "</head><body><h1>Room Environment Monitor</h1>";
  page += "<pre>" + info + "</pre>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

void checkWifiStatus() {
  // Print only when the connection changes, and start the server when connected.
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  if (isConnected != wasWifiConnected) {
    if (isConnected) {
      Serial.print("Ip Address: ");
      server.begin();
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Disconnected");
    }
    wasWifiConnected = isConnected;
  }
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

// Order: green, yellow, red. Set all three so the previous LED doesn't stay on.
void ledControl(bool greenOn, bool yellowOn, bool redOn) {
  digitalWrite(led_PinGreen, greenOn);
  digitalWrite(led_PinYellow, yellowOn);
  digitalWrite(led_PinRed, redOn);
}

void readEnvironment() {
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // A failed reading selects red and hides the old measurements on the page.
  if (dht.getStatus() != 0) {
    Serial.println(dht.getStatusString());
    ledControl(false, false, true);
    sensorError();
    environmentValid = false;
  } else {
    float fahrenheit;
    fahrenheit = (newValues.temperature * 1.8) + 32;
    Serial.println(" Temperature:" + String(newValues.temperature) + "°C" + " Temperature Fahrenheit:"
                   + String(fahrenheit) + "°F" + " Humidity:" + String(newValues.humidity) + "%");
    bool tempInRange = newValues.temperature >= minimumTempC && newValues.temperature <= maxTempC;
    bool humidityInRange = newValues.humidity >= minimumHumidity && newValues.humidity <= maxHumidity;
    if (tempInRange && humidityInRange) {
      ledControl(true, false, false);
    } else {
      ledControl(false, true, false);
    }
    updateScreen(newValues.temperature, newValues.humidity, fahrenheit);
    latestTemperatureC = newValues.temperature;
    latestTemperatureF = fahrenheit;
    latestHumidity = newValues.humidity;
    environmentValid = true;
  }
}

void updateLcdBacklight() {
  // Leave the backlight on while the PIR warms up.
  if (millis() - pirStartTime < pirWarmupInterval) {
    return;
  }
  // Only send a backlight command when its state needs to change.
  if (occupied != lcdBacklightOn) {
    if (occupied) {
      lcd.backlight();
    } else {
      lcd.noBacklight();
    }
    lcdBacklightOn = occupied;
  }
}

void readMotion() {
  // Ignore PIR readings during warm-up. The rest of the monitor keeps running.
  if (millis() - pirStartTime < pirWarmupInterval) {
    return;
  }
  int currentMotionState;
  currentMotionState = digitalRead(pir);
  if (currentMotionState != previousMotionState) {
    Serial.print("PIR: ");
    Serial.println(currentMotionState);
    previousMotionState = currentMotionState;
  }
  if (currentMotionState == 1) {
    bool wasOccupied = occupied;
    occupied = true;
    // Keep refreshing the timer for as long as the PIR stays HIGH.
    lastMotionTime = millis();
    if (wasOccupied != occupied) {
      Serial.println("Occupancy: Occupied");
    }
  } else if (occupied && millis() - lastMotionTime >= occupancyTimeout) {
    // No recent HIGH reading: clear occupancy after the timeout.
    bool wasOccupied = occupied;
    occupied = false;
    if (wasOccupied != occupied) {
      Serial.println("Occupancy: No Recent Movement");
    }
  }
}

void loop() {
  if (millis() - lastReadTime >= readInterval) {
    // Wait two seconds between attempts, even if the last read failed.
    lastReadTime = millis();
    readEnvironment();
  }
  // Check motion and browser requests between sensor reads.
  readMotion();
  updateLcdBacklight();
  checkWifiStatus();
  server.handleClient();
}
