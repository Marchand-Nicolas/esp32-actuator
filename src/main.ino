// Import environnement variables
#include "env.h"

// ESP32
#include <esp_sleep.h>

#include <ESP32Servo.h>

// GxEPD2_MinimumExample.ino by Jean-Marc Zingg

// purpose is e.g. to determine minimum code and ram use by this library

// see GxEPD2_wiring_examples.h of GxEPD2_Example for wiring suggestions and examples
// if you use a different wiring, you need to adapt the constructor parameters!

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX, to use less code and ram
// #include <GFX.h>

// #include <OneWire.h>

#include <GxEPD2_BW.h> // including both doesn't use more code or ram
#include <GxEPD2_3C.h> // including both doesn't use more code or ram

// select the display class and display driver class in the following file (new style):
#include "GxEPD2_display_selection_new_style.h"

// HTTP :
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

// APIs (process JSON responses)
#include <ArduinoJson.h>

// Medium
#include <Fonts/FreeMono9pt7b.h>

#include "ESPAsyncWebServer.h"

// 2 minutes timeout
const uint16_t TIMEOUT = 16959;

HTTPClient http;

// WiFi
WiFiMulti wifiMulti;

Servo servo;

AsyncWebServer server(80);

void openDoor()
{
  servo.write(targetOrientation);
  delay(rotationDuration);
  servo.write(defaultOrientation);
}

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    // request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("1");
    request->send(response);
    openDoor();
  }
};

void setup()
{
  if (debugEnabled)
  {
    Serial.begin(115200);
    Serial.println("Starting...");
  }

  // Screen
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  low_power();

  // esp_sleep_enable_wifi_wakeup();

  servo.attach(15);

  servo.write(defaultOrientation);

  if (debugEnabled)
    Serial.println("Wakeup reason: " + String(wakeup_reason));

  // Wifi
  wifiMulti.addAP(wifiSSID, wifiPassword);

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_TIMER:
    break;
  case ESP_SLEEP_WAKEUP_WIFI:
    break;
  default:
    // Screen
    display.init();
    display.setFullWindow();
    // Horizontal
    display.setRotation(1);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMono9pt7b);
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(20, 20);
    display.print("Telecommande 2000!!!");
    display.display(true);
    display.powerOff();
    break;
  }
  if (wifiMulti.run() == WL_CONNECTED)
  {
    if (debugEnabled)
      Serial.println("Connected to WiFi");
    // Clear screen
    pollServer();
  }
  server.addHandler(new CaptiveRequestHandler());
  server.begin();
}

void loop()
{
  if (debugEnabled)
    Serial.println("Refreshing...");
  pollServer();
  if (debugEnabled)
    Serial.println("Going to sleep...");
}

void pollServer()
{
  int chargeLevel = refreshBattery();
  String url = String(apiURL) + "/poll?battery=" + String(chargeLevel);
  http.begin(url);
  http.setTimeout(TIMEOUT);
  int httpCode = http.GET();
  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      if (payload == "true")
        openDoor();
    }
    else if (debugEnabled)
    {
      Serial.println("Error on HTTP request: " + httpCode);
    }
  }
  else if (debugEnabled)
    Serial.println("Error on HTTP request: " + httpCode);

  http.end();
}

int refreshBattery()
{
  // Battery
  // Bottom right
  int chargeLevel = getChargeLevelFromConversionTable(2.08 * getRawVoltage());
  if (debugEnabled)
    Serial.println("Charge level: " + String(chargeLevel));
  return chargeLevel;
}

double conversionTable[] = {3.200, 3.250, 3.300, 3.350, 3.400, 3.450, 3.500, 3.550, 3.600, 3.650, 3.700,
                            3.703, 3.706, 3.710, 3.713, 3.716, 3.719, 3.723, 3.726, 3.729, 3.732,
                            3.735, 3.739, 3.742, 3.745, 3.748, 3.752, 3.755, 3.758, 3.761, 3.765,
                            3.768, 3.771, 3.774, 3.777, 3.781, 3.784, 3.787, 3.790, 3.794, 3.797,
                            3.800, 3.805, 3.811, 3.816, 3.821, 3.826, 3.832, 3.837, 3.842, 3.847,
                            3.853, 3.858, 3.863, 3.868, 3.874, 3.879, 3.884, 3.889, 3.895, 3.900,
                            3.906, 3.911, 3.917, 3.922, 3.928, 3.933, 3.939, 3.944, 3.950, 3.956,
                            3.961, 3.967, 3.972, 3.978, 3.983, 3.989, 3.994, 4.000, 4.008, 4.015,
                            4.023, 4.031, 4.038, 4.046, 4.054, 4.062, 4.069, 4.077, 4.085, 4.092,
                            4.100, 4.111, 4.122, 4.133, 4.144, 4.156, 4.167, 4.178, 4.189, 4.200};

int getChargeLevelFromConversionTable(double volts)
{
  int index = 50;
  int previousIndex = 0;
  int half = 0;

  while (previousIndex != index)
  {
    half = abs(index - previousIndex) / 2;
    previousIndex = index;
    if (conversionTable[index] == volts)
    {
      return index;
    }
    index = (volts >= conversionTable[index])
                ? index + half
                : index - half;
  }

  return index;
}

double getRawVoltage()
{
  // read analog and make it more linear
  double reading = analogRead(35);
  for (int i = 0; i < 10; i++)
  {
    reading = (analogRead(35) + reading * 9) / 10;
  }
  if (reading < 1 || reading > 4095)
    return 0;

  return -0.000000000000016 * pow(reading, 4) + 0.000000000118171 * pow(reading, 3) - 0.000000301211691 * pow(reading, 2) + 0.001109019271794 * reading + 0.034143524634089;
}

void hibernate(uint64_t duration)
{
  // Configure Timer as wakeup source
  esp_sleep_enable_timer_wakeup(duration);
  esp_deep_sleep_start();
}

void light_sleep()
{
  esp_light_sleep_start();
}

void low_power()
{
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
}