#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <esp_task_wdt.h>

#include "secrets.h"

#define LED_PIN 2
#define RX_PIN 16 
#define TX_PIN 17

#define UPLD_INDEX 0
#define TEMP_INDEX 1
#define PRES_INDEX 2
#define HUMI_INDEX 3
#define RAIN_INDEX 4
#define WIND_INDEX 5
#define WDIR_INDEX 6
#define GUST_INDEX 7
#define GDIR_INDEX 8

#define MAX_PARAMS 15
 
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* stationID = STATION_ID;
const char* stationPassword = STATION_PASSWORD;

int splitString(const String& str, char delimiter, String* result, int maxParams) {
    int count = 0;
    int startIndex = 0;
    int delimiterIndex = str.indexOf(delimiter);

    while (delimiterIndex >= 0 && count < maxParams) {
        result[count++] = str.substring(startIndex, delimiterIndex);
        startIndex = delimiterIndex + 1;
        delimiterIndex = str.indexOf(delimiter, startIndex);
    }
    if (count < maxParams) {
        result[count++] = str.substring(startIndex); // Add the last segment
    }
    return count; // Return the number of segments
}

int cardinalToDegrees(const String& direction) {
    int degrees = -1; // Default value for invalid input
    switch (direction.charAt(0)) {
        case 'N':
            if (direction.substring(0,2) == "N") {
                degrees = 0;
            } else if (direction.substring(0,2) == "NE") {
                degrees = 45;
            } else if (direction.substring(0,2) == "NW") {
                degrees = 315;
            }
            break;
        case 'S':
            if (direction.substring(0,2) == "S") {
                degrees = 180;
            } else if (direction.substring(0,2) == "SE") {
                degrees = 135;
            } else if (direction.substring(0,2) == "SW") {
                degrees = 225;
            }
            break;
        case 'E':
            if (direction.substring(0,2) == "E") {
                degrees = 90;
            }
            break;
        case 'W':
            if (direction.substring(0,2) == "W") {
                degrees = 270;
            }
            break;
        default:
            degrees = -1;
            break;
    }
    return degrees;
}
 
void setup() {
  pinMode(LED_PIN,OUTPUT);
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // 9600 baud rate, 8 data bits, no parity, 1 stop bit
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting to WiFi Network ...");
  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(100);
  }
  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  String inputString;
  String values[MAX_PARAMS];
  float temp, pres, humi, rain, wind;
  int wind_direction_degrees, gust_direction_degrees;

  if (Serial.available()) {
    inputString = Serial.readStringUntil('\n');
    Serial.print("Received from USB Serial: ");
    Serial.println(inputString);
  }

  if (Serial1.available()) {
    inputString = Serial1.readStringUntil('\n');
    Serial.print("From Microbit: ");
    Serial.println(inputString);

    int count = splitString(inputString, ',', values, MAX_PARAMS);
    if (count <= MAX_PARAMS) {
      wind_direction_degrees = cardinalToDegrees(values[WDIR_INDEX]);
      gust_direction_degrees = cardinalToDegrees(values[GDIR_INDEX]);

      // Prepare the URL for Weather Underground
      String url = "https://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?";
      url += "ID=" + String(stationID);
      url += "&PASSWORD=" + String(stationPassword);
      url += "&dateutc=now";
      url += "&action=updateraw";
      url += "&tempf=" + values[TEMP_INDEX];
      url += "&humidity=" + values[HUMI_INDEX];
      url += "&windspeedmph=" + values[WIND_INDEX];
      if (wind_direction_degrees != -1) {
        url += "&winddir=" + String(wind_direction_degrees);
      }
      url += "&windgustmph=" + values[GUST_INDEX];
      url += "&windgustmph_10m=" + values[GUST_INDEX];
      if (gust_direction_degrees != -1) {
        url += "&windgustdir_10m=" + String(gust_direction_degrees);
        url += "&windgustdir=" + String(gust_direction_degrees);
      }
      url += "&rainin=" + values[RAIN_INDEX];
      url += "&realtime=1&rtfreq=30";
      // url += "&baromin=" + values[PRES_INDEX]; // I don't trust the barometric pressure number

      Serial.println(url);

      // Make the GET request
      if (WiFi.status() == WL_CONNECTED) {
        if (values[UPLD_INDEX] == "1") {
          digitalWrite(LED_PIN,HIGH); // Turn on Blue LED
          HTTPClient http;
          http.begin(url); 
          int httpResponseCode = http.GET(); 
          if (httpResponseCode > 0) {
              String response = http.getString(); 
              Serial.println("Response code: " + String(httpResponseCode));
              Serial.println("Response: " + response);
          } else {
              Serial.print("Error on HTTP request: ");
              Serial.println(httpResponseCode);
          }
          http.end();
        } else {
          Serial.println("Upload Disabled by Microbit");
          digitalWrite(LED_PIN,LOW); // Turn off Blue LED
        }
      } else {
        Serial.println("WiFi not connected.");
        digitalWrite(LED_PIN,LOW); // Turn off Blue LED
        WiFi.disconnect();
        Serial.println("Attempting to reconnect to WiFi ...");
        WiFi.begin(ssid, password);
        while(WiFi.status() != WL_CONNECTED){
          Serial.print(".");
          delay(100);
        }
        Serial.println("\WiFi Reconnected");
      }
    }
  }
  delay(100);
}