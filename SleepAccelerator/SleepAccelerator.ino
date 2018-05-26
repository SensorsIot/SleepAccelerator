/*
  Works on ESP8266

  Reads today's sunrise and sunset times as well as actual time and sends a "day" command to a LED lamp if
  the actual time is between sunrise and sunset. Otherwise it sends a night command. These commands change the color
  temperature of the lamp to adjust for change of color temperature of the sun
Please include your coordinates to get the right times for sunset and sunrise. No adjustment to timezones needed.

Based on ArduinoJson and SNTPtime library as well as the IRremoteESP8266 library.
https://github.com/markszabo/IRremoteESP8266
https://github.com/SensorsIot/SNTPtime
https://github.com/bblanchon/ArduinoJson

  Copyright <2018> <Andreas Spiess>

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Based on the Examples of the libraries
*/


#include <IRremoteESP8266.h>
#include <IRsend.h>
//#include <credentials.h>
#include <ArduinoJson.h>
#include <SNTPtime.h>

#include <ESP8266WiFi.h>
// #include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>


#define IR_LED D2

int SLEEPTIME = 30 * 1000000 * 60;

#define L6000K 0x4DB29966
#define L3000K 0x4DB2837C

IRsend irsend(IR_LED);  // Set the GPIO to be used to sending the message.

#ifdef CREDENTIALS
char *ssid      = mySSID;               // Set you WiFi SSID
char *password  = myPASSWORD;           // Set you WiFi password
#else
char *ssid      = "";               // Set you WiFi SSID
char *password  = "";               // Set you WiFi password
#endif

SNTPtime NTPch("ch.pool.ntp.org");

// ESP8266WiFiMulti wifiMulti;
/*
   The structure contains following fields:
   struct strDateTime
  {
  byte hour;
  byte minute;
  byte second;
  int year;
  byte month;
  byte day;
  byte dayofWeek;
  boolean valid;
  };
*/
strDateTime dateTime;
int minutesSunrise;
int minutesSunset;

char json[] = "";


int minutesInDay(const char* tim) {
  Serial.println(tim);
  int i = 0;
  int res = 0;
  while (tim[i] != 0) {
    if (tim[i] == ':') {
      int hi1 = tim[i - 1] - '0';
      int hi2 = tim[i - 2] - '0';
      if (i < 2) res = (60 * hi1); // single digit hour
      if (i == 2) res = 60 * (10 * hi2 + hi1);
      if (i > 2) res = res + (10 * hi2 + hi1);
    }
    i++;
  }
  if (tim[i - 2] == 'P') res = res + 720;  //PM
  return res;
}

void getTimes() {
  HTTPClient http;

  Serial.print("[HTTP] begin...\n");
  http.begin("http://api.sunrise-sunset.org/json?lat=47.471193&lng=7.750602&date=today"); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      JsonObject& root = jsonBuffer.parseObject(http.getString());
      // Parameters
      int id = root["id"]; // 1
      JsonObject& results = root["results"];
      const char* sunrise = results["sunrise"];
      const char* sunset = results["sunset"];

      // Output to serial monitor
      Serial.print("Sunrise: ");
      Serial.println(sunrise);
      Serial.print("Sunset: ");
      Serial.println(sunset);
      minutesSunrise = minutesInDay(sunrise);
      minutesSunset = minutesInDay(sunset);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void connectToWifi() {
  Serial.println();
  Serial.println("Connecting to wifi: ");
  Serial.println(ssid);
  // flush() is needed to print the above (connecting...) message reliably,
  // in case the wireless connection doesn't go through
  Serial.flush();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected");
}

void setup()
{
  Serial.begin(115200);
  //  Serial.setDebugOutput(true);
  Serial.println();
  irsend.begin();
  connectToWifi();
  NTPch.setSNTPtime();
  Serial.println("Got NTP-Time");
  getTimes();
  dateTime = NTPch.getTime(0.0, 0); // get time from internal clock
  NTPch.printDateTime(dateTime);
  int actMinutes = 60 * dateTime.hour + dateTime.minute;
  Serial.println(minutesSunrise);
  Serial.println(actMinutes);
  Serial.println(minutesSunset);
  if ((actMinutes - minutesSunrise) > 0 && (actMinutes - minutesSunset) < 0) {
    Serial.println("Day");
    for (int i = 0; i < 8; i++) {
      irsend.sendNEC(L6000K, 32);
      delay(200);
    }
  } else {
    Serial.println("Night");
    for (int i = 0; i < 8; i++) {
      irsend.sendNEC(L3000K, 32);
      delay(200);
    }
  }
  Serial.println("Going into deep sleep");
  ESP.deepSleep(SLEEPTIME); 

void loop() {
  yield();
}
