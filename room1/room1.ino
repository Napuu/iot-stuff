#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiClientSecureBearSSL.h>
#include "ESP8266WiFi.h"
#include <ESP8266HTTPClient.h>

#include "Secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int buttonPin1 = 15;
const int buttonPin2 = 13;
const int buttonPin3 = 12;
int buttonState1 = 0;
int buttonState2 = 0;
int buttonState3 = 0;

const unsigned long HUE_CALL_MIN_INTERVAL_MILLISECONDS = 200;
const unsigned long INFLUXDB_CALL_MIN_INTERVAL_MILLISECONDS = 30000;
const unsigned long DISPLAY_TOGGLE_INTERVAL_MILLISECONDS = 1000;
unsigned long currentTime = 0;
unsigned long lastHueCallTime = 0;
unsigned long lastInfluxdbCallTime = 0;
unsigned long lastDisplayToggleTime = 0;

bool clearDisplay = false;

void setup()
{
  Serial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  pinMode(buttonPin1, INPUT);
  pinMode(buttonPin2, INPUT);
  pinMode(buttonPin3, INPUT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
 
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  // lower brightness
  // however doens't seem to do much
  display.ssd1306_command(0x81);
  display.ssd1306_command(0);

  display.println(F("starting up..."));
  display.display();

  display.clearDisplay();
}
const int ELEMENT_COUNT_MAX = 10;

const int maxRows = 10;
char rows[maxRows][256];
const int maxCols = 15;
char cols[maxCols][256];

void splitString(const String& str, char delimiter, char target[][256]) {
  int stringCount = 0;
  int currentIndex = 0;

  for(int i = 0; i < str.length(); i++){
    char c = str.charAt(i);
    if(c == delimiter){
      strncpy(target[currentIndex], str.substring(stringCount, i).c_str(), sizeof(target[currentIndex]) - 1);
      target[currentIndex][sizeof(target[currentIndex]) - 1] = '\0';
      stringCount = i + 1;
      currentIndex++;
    }
  }
  strncpy(target[currentIndex], str.substring(stringCount).c_str(), sizeof(target[currentIndex]) - 1);
  target[currentIndex][sizeof(target[currentIndex]) - 1] = '\0';
}


int request(char url[], const char* contentType, const char* payload) {
  Serial.println("starting request");
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-type", contentType);
  int httpCode;
  httpCode = http.PUT(payload);
  http.end();
  return httpCode;
}

const char lightScenes[][48]  = {
  HUE_LIGHT_SCENE1,
  HUE_LIGHT_SCENE2,
  HUE_LIGHT_SCENE3
};

void setLightScene(int sceneIndex) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, HUE_BRIDGE_URL);
  http.addHeader("Content-type", "application/json");
  int httpCode;
  httpCode = http.PUT(lightScenes[sceneIndex]);
  http.end();
}

const size_t valueCount = 3;
const char targetMeasurements[][12] = {
  "temperature",
  "humidity",
  "pressure"
};
char targetValues[valueCount][7];
void resetTargetValues() {
  for (size_t i = 0; i < valueCount; i++) {
    strcpy(targetValues[i], "-");
  }
}

void updateMeasurements() {
  if (lastInfluxdbCallTime != 0 && currentTime - lastInfluxdbCallTime < INFLUXDB_CALL_MIN_INTERVAL_MILLISECONDS) {
    return;
  }
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  HTTPClient https;
  client->setInsecure();
  https.begin(*client, INFLUX_URL);
  https.addHeader("Content-type", "application/vnd.flux");
  https.addHeader("Accept", "application/csv");
  https.addHeader("Authorization", INFLUX_AUTHORIZATION_HEADER);
  int httpCode = https.POST(INFLUX_QUERY);
  if (httpCode < 200 || httpCode >= 400) {

    // resetting values works as an indicator that something
    // went wrong when serial is not attached
    resetTargetValues();
    Serial.println("failed to get measurements");

    return;
  }
  String payload = https.getString();
  memset(rows, 0, sizeof(rows));
  splitString(payload, '\n', rows);
  // Serial.println(ESP.getFreeHeap());
  int fieldIndex = -1, measurementIndex = -1;
  splitString(payload, '\n', rows);
  int i = 0;
  while (i < maxRows) {
    char sep = ',';
    memset(cols, 0, sizeof(cols));
    splitString(rows[i], sep, cols);
    if (rows[i][0] == '\0') {
      break;
    }
    for (int j = 0; j < maxCols; j++) {
      if (strcmp(cols[j], "_field") == 0) fieldIndex = j-1;
      if (strcmp(cols[j], "_measurement") == 0) measurementIndex = j-1;
      if (i > 0) {
        for (int k = 0; k < sizeof(targetMeasurements) / sizeof(targetMeasurements[0]); k++) {
          if (strncmp(cols[measurementIndex], targetMeasurements[k], strlen(targetMeasurements[k])) == 0) {
            float num = atof(cols[fieldIndex]);
            num = round(num * 10.0) / 10.0;
            sprintf(targetValues[k], "%.1f", num);
          }
        }
      }
    }
    i++;
  }
  lastInfluxdbCallTime = currentTime;
}

void setupDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
}

void displayMeasurements() {
  display.print(targetValues[0]);
  display.println(" C");

  display.print(targetValues[1]);
  display.println(" %");

  display.print(targetValues[2]);
  display.println(" kPa");

  int width = round((currentTime - lastInfluxdbCallTime) / ((float) INFLUXDB_CALL_MIN_INTERVAL_MILLISECONDS) * SCREEN_WIDTH);
  display.fillRect(0, 56, width, 2, 1);
  if (clearDisplay) {
    display.clearDisplay();
  }
  display.display();
}

void handleLightButtons() {
  if ((buttonState1 == 0 && buttonState2 == 0 && buttonState3 == 0) || currentTime - lastHueCallTime < HUE_CALL_MIN_INTERVAL_MILLISECONDS) {
    return;
  }

  Serial.println("states: " + String(buttonState1) + "," + String(buttonState2) + "," + String(buttonState3));
  if (buttonState1 == 1) {
    setLightScene(0);
  } else if (buttonState2 == 1) {
    setLightScene(1);
  } else if (buttonState3 == 1) {
    setLightScene(2);
  }
  lastHueCallTime = currentTime;
}

void handleDisplayToggleButtons() {
  if ((buttonState1 == 0 || buttonState2 == 0) || currentTime - lastDisplayToggleTime < DISPLAY_TOGGLE_INTERVAL_MILLISECONDS) {
    return;
  }
  lastDisplayToggleTime = currentTime;
  clearDisplay = !clearDisplay;
}

void readButtonValues() {
  buttonState1 = digitalRead(buttonPin1);
  buttonState2 = digitalRead(buttonPin2);
  buttonState3 = digitalRead(buttonPin3);
}

void loop() {
  currentTime = millis();

  setupDisplay();

  readButtonValues();
  handleLightButtons();
  handleDisplayToggleButtons();

  updateMeasurements();
  displayMeasurements();
}
