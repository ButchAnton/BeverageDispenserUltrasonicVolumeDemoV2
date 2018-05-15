// NOTE!!!!!!!
// You must increase the stack size for this program.
// If you are using PlatformIO, the stack size is defined in
// ~/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp
// The default value is 8192.
// For this program, I use 16384.

// #define CLEAR_SAVED_WIFI_SETTINGS 1

// NOTE!!!!!!!
// Before you ship the board (or, even better, when you first flash the board), make sure
// that you uncomment the define below so that the SPIFFS gets formatted and a default
// config file gets written.  This is the safest way to do things.  If you forget,
// the code should write one for you when it doesn't find one, but it's better
// not to rely on it.

// #define WRITE_SAMPLE_FILE 1

//  These define the application to which the code is applicable.  Undefine
// ONLY ONE of these.  Alternately, you can pass a -D flag on the compile
// line to define ONLY ONE of these.
// For example, -DLEONARDO_CENTERS=1
// If you're using PlatformIO, you can add this define to the build_flags
// section of platformio.ini.  This is the preferred way, since the code
// will fail to compile if nothing is set in platformio.ini.

// #define GOLDEN_DEMO 1
// #define LEONARDO_CENTERS 1

// Debug printing

#define DEBUG 1
#define debug_print(fmt, ...) \
  do { if (DEBUG) Serial.printf("%s:%d:%s(): " fmt, __FILE__, \
    __LINE__, __func__, ##__VA_ARGS__); } while (0)

#define debug_simple_print(fmt, ...) \
  do { if (DEBUG) Serial.printf(fmt, ##__VA_ARGS__); } while (0)

#include <Arduino.h>

// JSON parser

#include <ArduinoJson.h>

// NTP

#include <NTPClient.h>
#include <time.h>

// Wi-Fi and WiFiManager

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <DNSServer.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif
#include <WiFiManager.h>

#define AP_SSID "SAP_SensorAP"

// HTTP/REST

#include <HTTPClient.h>

// POST default values and time between POSTs.

int sensorPostReturnValue = 202;
#define POST_INTERVAL_SECONDS (20)

// Initialize the WiFiManager.  This library provides a captive portal for configuring
// Wi-Fi connective on the device, as well as on-device configuration and storage of
// random data, like endpoints, passwords, etc.

WiFiManager wifiManager;

// Allow the Prg button to clear Wi-Fi settings.

#define PRG_PIN 0
// If this is true, the prg button has been pushed and we need to restore the board
// to its default state.
bool restoreBoardToDefault = false;

// Ultrasonic distance sensor

#define TRIGGER_PIN 21
#define ECHO_PIN 22

const double speedOfSound = 2.93866995797702; // mm per microsecond
const double heightOfColumn = 573.388650;  // height of the beverage dispenser in mm
double levelPercentage = -100.00;
double previousLevelPercentage = 0.00;
double deltaValue = 0.00;
#define POST_LEVEL_DELTA (0.5)
int forcePostCount = 1;
#define MAX_SKIPPED_READINGS (6)

// File system

#include <FS.h>
#include <SPIFFS.h>

#define CONFIG_FILE "/config.json"
#define IOTS_ENDPOINT_SIZE 256
#define OAUTH_ENDPOINT_SIZE 256
#define OAUTH_CLIENT_ID_SIZE 128
#define OAUTH_CLIENT_SECRET_SIZE 128
#define SENSOR_ID_SIZE 128

#ifdef LEONARDO_CENTERS
#define IOTS_ENDPOINT_DEFAULT_VALUE "https://sap-connected-goods-ingestion-api-demo-cdemo.cfapps.eu10.hana.ondemand.com/ConnectedGoods/v1/DeviceData"
#define OAUTH_ENDPOINT_DEFAULT_VALUE "https://cng-leonardoc.authentication.eu10.hana.ondemand.com/oauth/token"
#define OAUTH_CLIENT_ID_DEFAULT_VALUE "sb-sap-connected-goods-cust-demo!t5"
#define OAUTH_CLIENT_SECRET_DEFAULT_VALUE "bc%2B1Zrm4%2Bc%2FZUuD0TXUUTxP5F0k%3D"
#define SENSOR_ID_DEFAULT_VALUE "SENSOR_1"
#endif // LEONARDO_CENTERS

#ifdef GOLDEN_DEMO
#define IOTS_ENDPOINT_DEFAULT_VALUE "https://sap-connected-goods-ingestion-api-qa.cfapps.eu10.hana.ondemand.com/ConnectedGoods/v1/DeviceData"
#define OAUTH_ENDPOINT_DEFAULT_VALUE "https://cng-qa1.authentication.eu10.hana.ondemand.com/oauth/token"
#define OAUTH_CLIENT_ID_DEFAULT_VALUE "sb-sap-connected-goods-qa!t5"
#define OAUTH_CLIENT_SECRET_DEFAULT_VALUE "Eu7gZ7suJG4VRahheB2i3F2E85E="
#define SENSOR_ID_DEFAULT_VALUE "LIVE_SILO_01"
#endif // GOLDEN_DEMO

const char *iots_endpoint_key = "iots_endpoint";
char iots_endpoint_value[IOTS_ENDPOINT_SIZE] = IOTS_ENDPOINT_DEFAULT_VALUE;
const char *oauth_endpoint_key = "oauth_endpoint";
char oauth_endpoint_value[OAUTH_ENDPOINT_SIZE] = OAUTH_ENDPOINT_DEFAULT_VALUE;
const char *oauth_client_id_key = "oauth_client_id";
char oauth_client_id_value[OAUTH_CLIENT_ID_SIZE] = OAUTH_CLIENT_ID_DEFAULT_VALUE;
const char *oauth_client_secret_key = "oauth_client_secret";
char oauth_client_secret_value[OAUTH_CLIENT_SECRET_SIZE] = OAUTH_CLIENT_SECRET_DEFAULT_VALUE;
const char *sensor_id_key = "sensor_id";
char sensor_id_value[SENSOR_ID_SIZE] = SENSOR_ID_DEFAULT_VALUE;
bool needToSaveConfiguration = false;

// Callback that gets triggered when configuration parameters are changed.

void saveConfigCallback() {
  needToSaveConfiguration = true;
  debug_print("callback called: needToSaveConfiguration set to true.\n");
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  char *filename;

  debug_print("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    debug_print("Failed to open directory.");
    return;
  }
  if (!root.isDirectory()) {
    debug_print("%s is not a directory.\n", dirname);
    return;
  }

  File file = root.openNextFile();
  while (file) {
    filename = (char *)file.name();
    if (file.isDirectory()) {
      debug_simple_print("  DIR : ");
      debug_simple_print("%s\n", filename);
      if (levels) {
        listDir(fs, filename, levels - 1);
      }
    } else {
      debug_simple_print("  FILE: ");
      debug_simple_print("%s", filename);
      int filesize = file.size();
      debug_simple_print("  SIZE: ");
      debug_simple_print("%d\n", filesize);
    }
    file = root.openNextFile();
  }
}

void writeDefaultConfigFile(bool shouldFormat) {

  debug_print("Writing default config file.\n");

  if (shouldFormat) {
    debug_print("Formatting SPIFFS.\n");
    SPIFFS.format();
  }

  if (SPIFFS.begin()) {

    debug_print("Mounted the filesystem.\n");

    // Write a test file.

    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json[iots_endpoint_key] = iots_endpoint_value;
    json[oauth_endpoint_key] = oauth_endpoint_value;
    json[oauth_client_id_key] = oauth_client_id_value;
    json[oauth_client_secret_key] = oauth_client_secret_value;
    json[sensor_id_key] = sensor_id_value;
    File configFile = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
    if (!configFile) {
      debug_print("Failed to open config file %s for writing.\n", CONFIG_FILE);
    }

#ifdef DEBUG
    json.printTo(Serial);
#endif // DEBUG
    debug_simple_print("\n");
    json.printTo(configFile);
    configFile.close();

    // Get the file size and print it.

    configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
    size_t configFileSize = configFile.size();
    debug_print("Wrote config file.  File size is %d bytes.\n", configFileSize);
    configFile.close();
    debug_print("Root level directory listing:\n");
    listDir(SPIFFS, "/", 0);
    SPIFFS.end();
  } else {
    debug_print("Unable to mount the filesystem.\n");
  }
}

// Display

#include <Wire.h>
#include "SSD1306.h"

// Absolutely disgusting macros to allow me to programmitically
// create a font name while at the same time being able to
// programmitically use the font size.  Ugh.
// It all works based on re-evaluating the macro to get the
// embedded macro value to expand.
// Having gone to all the trouble of doing this, I discovered
// that the font size doesn't actually correspond to its height
// in pixels, so this code doesn't really work anyway.  I'm keeping
// it around because it's a reasonably unique way of using macros.
// Upon examination of the font definitions, at least for the Ariel fonts,
// the font height is stored in position 1 of the array describing the font:
// ArialMT_Plain_10[1] = 13, for example.

# if 0
#define FONT_CHARACTER_HEIGHT 10
#define CREATE_DISPLAY_FONT(size) ArialMT_Plain_##size
#define TEMP_FONT CREATE_DISPLAY_FONT(FONT_CHARACTER_HEIGHT)
#define FONT(x) CREATE_DISPLAY_FONT(x)
#endif // 0

SSD1306 display(0x3c, 4, 15);

int fontHeight = -1;
#define LINE_SPACING(X) (fontHeight * X)
#define MAX_DISPLAY_STRING_LENGTH (25)
#define SECONDS_BETWEEN_SCREENS (3 * 1000)

// CNG server information

// String dataServer = F("https://sap-connected-goods-ingestion-api-qa.cfapps.eu10.hana.ondemand.com/ConnectedGoods/v1/DeviceData");
// String authServer = F("https://cng-qa1.authentication.eu10.hana.ondemand.com/oauth/token");

// Server certificate obtained from the server.
// $ openssl s_client -showcerts -connect <server:443> < /dev/null
// The last PEM-encoded certificate is the one that we want.  It is
// shorter than the Root CA certificate and is signed by the Root CA.

const char* serverCert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIElDCCA3ygAwIBAgIQAf2j627KdciIQ4tyS8+8kTANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaME0xCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg\n" \
"U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" \
"ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83\n" \
"nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd\n" \
"KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f\n" \
"/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX\n" \
"kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0\n" \
"/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8C\n" \
"AQAwDgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYY\n" \
"aHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6\n" \
"Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1\n" \
"oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RD\n" \
"QS5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8v\n" \
"d3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFA+AYRyCMWHVLyjnjUY4tCzh\n" \
"xtniMB8GA1UdIwQYMBaAFAPeUDVW0Uy7ZvCj4hsbw5eyPdFVMA0GCSqGSIb3DQEB\n" \
"CwUAA4IBAQAjPt9L0jFCpbZ+QlwaRMxp0Wi0XUvgBCFsS+JtzLHgl4+mUwnNqipl\n" \
"5TlPHoOlblyYoiQm5vuh7ZPHLgLGTUq/sELfeNqzqPlt/yGFUzZgTHbO7Djc1lGA\n" \
"8MXW5dRNJ2Srm8c+cftIl7gzbckTB+6WohsYFfZcTEDts8Ls/3HB40f/1LkAtDdC\n" \
"2iDJ6m6K7hQGrn2iWZiIqBtvLfTyyRRfJs8sjX7tN8Cp1Tm5gr8ZDOo0rwAhaPit\n" \
"c+LJMto4JQtV05od8GiG7S5BNO98pVAdvzr508EIDObtHopYJeS4d60tbvVS3bR0\n" \
"j6tJLp07kzQoH3jOlOrHvdPJbRzeXDLz\n" \
"-----END CERTIFICATE-----\n";

String oauthToken;

// NTP configuration Information

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);
char formatted_time[80];

// Drawing functions for various informational screens.

void drawSetupWiFiFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  display.drawString(0, LINE_SPACING(0), "Connect to SAP_SensorAP");
  display.drawString(0, LINE_SPACING(1), "with your phone or");
  display.drawString(0, LINE_SPACING(2), "laptop to configure");
  display.drawString(0, LINE_SPACING(3), "Wi-Fi and parameters.");
  display.display();
}

void drawWiFiClientInformationFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  display.drawString(0, LINE_SPACING(0), "Connected to Wi-Fi network:");
  display.drawStringMaxWidth(0, LINE_SPACING(1), 128, WiFi.SSID());
  String ipString = "IP: " + wifiManager.toStringIp(WiFi.localIP());
  display.drawStringMaxWidth(0, LINE_SPACING(2), 128, ipString);
  display.display();
}

void drawIotsEndpointInformationFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  String iotsString = "iots: " + String(iots_endpoint_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, iotsString);
  display.display();
}

void drawOauthEndpointInformationFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  String oauthString = "oauth: " + String(oauth_endpoint_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, oauthString);
  display.display();
}

void drawOauthClientIDInformationFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  String oauthClientIDString = "oauth clientID: " + String(oauth_client_id_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, oauthClientIDString);
  display.display();
}

void drawOauthClientSecretInformationFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  String oauthSecretString = "oauth secret: " + String(oauth_client_secret_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, oauthSecretString);
  display.display();
}

void drawSensorIDInformationFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  String sensorIDString = "sensorID: " + String(sensor_id_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, sensorIDString);
  display.display();
}

void drawSensorInformationFrame() {
  debug_print("Displaying frame.\n");
  display.clear();
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, formatted_time);
  String sensorString = "Fill %: " + String(levelPercentage) + "%";
  display.drawStringMaxWidth(0, LINE_SPACING(1), 128, sensorString);
  String postString = "POST returns: " + String(sensorPostReturnValue);
  display.drawStringMaxWidth(0, LINE_SPACING(2), 128, postString);
  display.display();
}

// Retrieve an OAuth token from SAP CNG for use with POST requests.

void getAuthToken() {

#define JSON_BUFFER_SIZE 4500

  String response = "";
  int returnCode = 0;
  HTTPClient http;

  http.begin(oauth_endpoint_value, serverCert);

  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String request = "client_id=" + String(oauth_client_id_value) + "&client_secret=" + String(oauth_client_secret_value) + "&grant_type=client_credentials&token_format=jwt&response_type=token";
  // debug_print("request: %s\n", request.c_str());

  // debug_print("Before calling POST\n");
  returnCode = http.POST(request);
  // debug_print("returnCode = %d\n", returnCode);
  // debug_print("After calling POST\n");

  if (returnCode == HTTP_CODE_OK) {
    // debug_print("Setting response\n");
    response = http.getString();
    // debug_print("response: %s\n", response.c_str());
    // response = "{\"access_token\":\"abcde\"}";
    // debug_print("allocating jsonBuffer<JSON_BUFFER_SIZE>\n");
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    // StaticJsonBuffer<100> jsonBuffer;
    // debug_print("calling parseObject\n");
    JsonObject& root = jsonBuffer.parseObject(response);
    // Test if parsing succeeds.
    if (!root.success()) {
      Serial.printf("parseObject() failed!\n");
      delay(100000000);
    }
    // debug_print("retrieving access_token\n");
    const char *token = root["access_token"];
    // debug_print("token = %s\n", token);
    // Serial.print("token: ");
    // Serial.println(token);
    // debug_print("Creating oauthToken\n");
    oauthToken = "Bearer " + String(token);
    debug_print("oauthToken = %s\n", oauthToken.c_str());
  } else {
    debug_print("Error getting auth token, returnCode = %d\n", returnCode);
  }
}

// Post the level of the beverage dispenser to CNG as a percentage of full.

void postLevelPercentage(float levelPercentage) {
  HTTPClient http;

  String response = "";
  sensorPostReturnValue = -999;

  // Get the time in this format: 2017-10-24T21:54:22.261Z

  time_t time_now;
  struct tm time_now_struct;

  timeClient.update();
  time_now = timeClient.getEpochTime();
  time_now_struct = *localtime(&time_now);
  // Serial.printf("time_now (epoch time) is %ld\n", time_now);
  strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%dT%H:%M:%SZ", &time_now_struct);
  // Serial.printf("The time is %s\n", formatted_time);

  // Get the current oAuth token.

  getAuthToken();

  // Set up the connection to the IoTS endpoint.

  http.begin(iots_endpoint_value, serverCert);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", oauthToken);

  // {"messages": [{"cng_deviceId": "Live-Container-1","timestamp":"2018-01-11T20:54:58.437Z","filllevel": 12,"expiryDate": "10/12/2022","messagetype": 700,"filltype": 1,"fragrance": "Ginger"}]}

  // String request = "{\"messages\":[{\"cng_deviceId\":\"" + sensorID + "\",\"timestamp\":\"" + formatted_time + "\",\"flavour\":1,\"fillLevel\":" + levelPercentage + ",\"longitude\":" + longitude + ",\"latitude\":" + latitude + ",\"temperature\":18.1}]}";
#ifdef LEONARDO_CENTERS
  http.addHeader("cng_devicetype", "ConnectedSilos");
  http.addHeader("cng_messagetype", "SILO_TIME_SERIES");
  String request = "{\"messages\":[{\"cng_deviceId\":\"" + String(sensor_id_value) + "\",\"timestamp\":\"" + formatted_time + "\",\"filllevel\":" + levelPercentage + ",\"expiryDate\":\"10/10/2022\",\"messagetype\":700,\"filltype\":1,\"fragrance\":\"Ginger\"}]}";
#endif // LEONARDO_CENTERS

#if 0
Below are the relevant details for our setup.

Application URL: https://cng-qa1-ingqa.ing-sap.cfapps.eu10.hana.ondemand.com/
AUTH URL : httphttps://cng-qa1-ingqa.ing-sap.cfapps.eu10.hana.ondemand.com/oauth/token
"clientid": "sb-sap-connected-goods-qa!t5",
"clientsecret": "Eu7gZ7suJG4VRahheB2i3F2E85E=",

Device Data Post URL : https://sap-connected-goods-ingestion-api-qa.cfapps.eu10.hana.ondemand.com/ConnectedGoods/v1/DeviceData

Headers
cng_devicetype: ConnectedSilos
cng_messagetype: SILO_TIME_SERIES

Payload
{  "messages":
[
{
"cng_deviceId":”LIVE_SILO_01”,
"timestamp": "2018-01-11T20:54:58.437Z",
"fillLevelTons":  2,        ( Liters- From the sensor)
"longitude": à @Caswell, Bob could provide this.,
"temperature": From sensor ,
"content": inputs from @Caswell, Bob
"latitude": @Caswell, Bob could provide this.,
"fillLevel": this has to be calculated as a percentage – fillLevelTons * 100 / 3 = 2 *100 / 3 = 66.66
"humidity": Can be any value.

}
]
}
#endif // 0

#ifdef GOLDEN_DEMO
#define TANK_CAPACITY (3.3) // liters
#define SCCC_LATITUDE "37.4046706"
#define SCCC_LONGITUDE "-121.9774409"
#define CONTENT "Beer"
#define TEMPERATURE "5.0"
#define RELATIVE_HUMIDITY "35.5"
  http.addHeader("cng_devicetype", "ConnectedSilos");
  http.addHeader("cng_messagetype", "SILO_TIME_SERIES");
  String request = "{\"messages\":[{\"cng_deviceId\":\"" + String(sensor_id_value) + "\",\"longitude\":\"" + String(SCCC_LONGITUDE) + "\",\"latitude\":\"" + String(SCCC_LATITUDE) + "\",\"content\":\"" + String(CONTENT) + "\",\"temperature\":\"" + String(TEMPERATURE) + "\",\"humidity\":\"" + String(RELATIVE_HUMIDITY) + "\",\"timestamp\":\"" + formatted_time + "\",\"fillLevel\":" + levelPercentage + ",\"fillLevelTons\":" + (levelPercentage * TANK_CAPACITY / 100.0) + "}]}";
#endif // LEONARDO_CENTERS

  debug_print("Oauth token: <BEGIN TOKEN>%s<END TOKEN>, length = %d\n", oauthToken.c_str(), oauthToken.length());
  debug_print("Host: %s Request: %s\n", iots_endpoint_value, request.c_str());
  sensorPostReturnValue = http.POST(request);
  if (sensorPostReturnValue == HTTP_CODE_OK || sensorPostReturnValue == HTTP_CODE_ACCEPTED) {
    response = http.getString();
    debug_print("Response: %s, returnCode = %d\n", response.c_str(), sensorPostReturnValue);
  } else {
    debug_print("Error posting sensor data, returnCode = %d\n", sensorPostReturnValue);
  }
}

// Read the distance sensor, determine the distance to the fluid, compute
// the percentage full of the dispenser, and return it.

double getFillPercentage() {

  unsigned long duration;
  float distance;
  float percentage;

  // Drop the trigger pin low for 2 microseconds

  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);

  // Send a 10 microsecond burst (high) on the trigger pin

  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  // Read the echo pin, which is the round trip time in microseconds.

  duration = pulseIn(ECHO_PIN, HIGH);

  // Calculate the distance in mm.  This is half of the duration
  // divided by the speed of sound in microseconds.

  distance = (duration / 2.0) / speedOfSound;
  percentage = (100.00 - ((distance / heightOfColumn) * 100.0));

  debug_print("Duration: %5.2lu\tDistance: %7.2f mm\t fillLevel: %3.2f%%\n", duration, distance, percentage);

  return(percentage);
}

void prgButtonPushed() {
  restoreBoardToDefault = true;
}

void resetWiFiAndBoard() {
  debug_print("Prg button pressed -- resetting board to default state.\n");
  debug_print("Clearing stored Wi-Fi information.\n");
  WiFi.disconnect(true);
  writeDefaultConfigFile(true);
  debug_print("Restarting board.\n");
  ESP.restart();
}

void setup() {
    Serial.begin(115200);
    debug_print("Booting (10 seconds) ...\n");
    delay(10000);

#ifdef GOLDEN_DEMO
  debug_print("Using GOLDEN_DEMO\n");
#endif // GOLDEN_DEMO

#ifdef LEONARDO_CENTERS
  debug_print("Using LEONARDO_CENTERS\n");
#endif // LEONARDO_CENTERS

#ifdef WRITE_SAMPLE_FILE
  writeDefaultConfigFile(true);
  debug_print("Recompile the code, turning off the WRITE_SAMPLE_FILE flag.\n");
  while (true) {;}
#endif // WRITE_SAMPLE_FILE

    // Allow pressing the "Prg" button to reset the Wi-Fi information.  This reboots
    // the board.

    pinMode(PRG_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PRG_PIN), prgButtonPushed, FALLING);

    // Enable the HC-SR04 pins for the ultrasonic distance sensor.

    pinMode(TRIGGER_PIN, OUTPUT); // Sets the trigger pin as an output.
    pinMode(ECHO_PIN, INPUT); // Sets the echo pin as an input.

    // Change the screen orientation and choose a small, good looking font.
    // Display the Wi-Fi setup screen.  This will be displayed until Wi-Fi
    // is configured.  I hope.

    display.setFont(ArialMT_Plain_10);
    fontHeight = ArialMT_Plain_10[1] - 4; // Tighten things up a bit.
    display.init();
    display.flipScreenVertically();
    drawSetupWiFiFrame();

    // Retrieve custom parameters.  Do this before anything else, since we'll
    // need them to populate the WiFiManager portal page.

    // Mount the filesystem.

    if (SPIFFS.begin()) {

      debug_print("Mounted the filesystem.\n");

      // Read the config file if it exists.

      if (SPIFFS.exists(CONFIG_FILE)) {

        debug_print("Opening the config file %s.\n", CONFIG_FILE);
        File configFile = SPIFFS.open(CONFIG_FILE, "r");  // open for reading only

        if (configFile) {

          debug_print("Opened file successfully for reading.");
          size_t configFileSize = configFile.size();
          debug_print("File size is %d bytes.\n", configFileSize);

          // Allocate a buffer into which to read the whole file.

          char *configBuffer = (char *)malloc(configFileSize * sizeof(char));

          // Read the file into the buffer.

          int bytesRead = configFile.readBytes(configBuffer, configFileSize);
          debug_print("Read %d bytes from the config file.\n", bytesRead);
          debug_print("String length of contents: %d\n", strlen(configBuffer));
          debug_print("Contents: %s\n", configBuffer);
          configBuffer[bytesRead] = '\0';
          debug_print("Contents (with null): %s\n", configBuffer);

          // Parse the buffer.

          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(configBuffer);
          json.printTo(Serial);
          debug_simple_print("\n");
          if (json.success()) {
            debug_print("Successfully parsed json.\n");
            strcpy(iots_endpoint_value, json[iots_endpoint_key]);
            debug_print("%s = %s\n", iots_endpoint_key, iots_endpoint_value);
            strcpy(oauth_endpoint_value, json[oauth_endpoint_key]);
            debug_print("%s = %s\n", oauth_endpoint_key, oauth_endpoint_value);
            strcpy(oauth_client_id_value, json[oauth_client_id_key]);
            debug_print("%s = %s\n", oauth_client_id_key, oauth_client_id_value);
            strcpy(oauth_client_secret_value, json[oauth_client_secret_key]);
            debug_print("%s = %s\n", oauth_client_secret_key, oauth_client_secret_value);
            strcpy(sensor_id_value, json[sensor_id_key]);
            debug_print("%s = %s\n", sensor_id_key, sensor_id_value);
          } else {
            debug_print("Failed to load json config.");
          }
        } // Read the config file

      } else {
        debug_print("Config file %s does not exist.\n", CONFIG_FILE);
      }

    } else {
      // If we couldn't mount the SPIFFS, format the file system and write a default config file.
      debug_print("Failed to mount SPIFFS!!!!!!\n");
      writeDefaultConfigFile(true);
    }

    // Uncomment this line to erase saved settings.  Note that it does not get rid of
    // previously stored SSIDs/passwords.  See the comment below.

    // wifiManager.resetSettings();

    // If you want to configure your portal to have a custom IP address, etc.,
    // use the line below.

    // wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    // Set up the configuration parameters.

    WiFiManagerParameter custom_iots_endpoint_value("IoTS Endoint", "Custom IoTS Endpoint", iots_endpoint_value, IOTS_ENDPOINT_SIZE);
    WiFiManagerParameter custom_oauth_endpoint_value("OAuth Endpoint", "Custom OAuth Endpoint", oauth_endpoint_value, OAUTH_ENDPOINT_SIZE);
    WiFiManagerParameter custom_oauth_client_id_value("OAuth Client ID", "Custom OAuth Client ID", oauth_client_id_value, OAUTH_CLIENT_ID_SIZE);
    WiFiManagerParameter custom_oauth_client_secret_value("OAuth Client Secret", "Custom OAuth Client Secret", oauth_client_secret_value, OAUTH_CLIENT_SECRET_SIZE);
    WiFiManagerParameter custom_sensor_id_value("Sensor ID", "Custom Sensor ID", sensor_id_value, SENSOR_ID_SIZE);
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&custom_iots_endpoint_value);
    wifiManager.addParameter(&custom_oauth_endpoint_value);
    wifiManager.addParameter(&custom_oauth_client_id_value);
    wifiManager.addParameter(&custom_oauth_client_secret_value);
    wifiManager.addParameter(&custom_sensor_id_value);

    // Retrieve any stored SSIDs/passwords and try to connect to them.
    // If all else fails, start a Wi-Fi network with the SSID given.
    // Users should connect to this SSID and (perhaps, if their device does not
    // automagically direct them to the captive portal) use a browser to connect
    // to a website.  If this fails, and the IP address of the AP wasn't specified
    // above, they should try to browse to 192.168.4.1.

    wifiManager.autoConnect(AP_SSID);

    // Use this variant to have an SSID auto generated.  The same rules as above
    // apply.

    // wifiManager.autoConnect();

    // We have connected to the Wi-Fi network.  Print some information and carry
    // on.

    debug_print("Connected to SSID %s, IP %s\n", WiFi.SSID().c_str(), wifiManager.toStringIp(WiFi.localIP()).c_str());

    // Now that we're connected, display the new Wi-Fi and configuration information.

    // Get the custom parameters from the configuration page.

    strcpy(iots_endpoint_value, custom_iots_endpoint_value.getValue());
    strcpy(oauth_endpoint_value, custom_oauth_endpoint_value.getValue());
    strcpy(oauth_client_id_value, custom_oauth_client_id_value.getValue());
    strcpy(oauth_client_secret_value, custom_oauth_client_secret_value.getValue());
    strcpy(sensor_id_value, custom_sensor_id_value.getValue());

    // If we changed any paramater configuration, save it.

    if (needToSaveConfiguration) {
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json[iots_endpoint_key] = iots_endpoint_value;
      json[oauth_endpoint_key] = oauth_endpoint_value;
      json[oauth_client_id_key] = oauth_client_id_value;
      json[oauth_client_secret_key] = oauth_client_secret_value;
      json[sensor_id_key] = sensor_id_value;
      File configFile = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
      if (!configFile) {
        debug_print("Failed to open config file %s for writing.\n", CONFIG_FILE);
      }

#ifdef DEBUG
      json.printTo(Serial);
      debug_simple_print("\n");
#endif // DEBUG
      json.printTo(configFile);
      configFile.close();

      // Get the file size and print it.

      configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
      size_t configFileSize = configFile.size();
      debug_print("Wrote config file.  File size is %d bytes.\n", configFileSize);
      configFile.close();
      debug_print("Root level directory listing:\n");
      listDir(SPIFFS, "/", 0);

    }

    // If you want to clear the saved SSIDs/passwords, uncomment the following
    // two lines.  This is specific to the ESP32 and is a current limitation.
    // Hopefully it will be fixed in the future.  Note that this only works
    // if you have successfully connected to a Wi-Fi network.

#ifdef CLEAR_SAVED_WIFI_SETTINGS
    debug_print("Clearing the saved SSID/password information.");
    WiFi.disconnect(true);
#endif // CLEAR_SAVED_WIFI_SETTINGS

  // Display the information to the screen.  Make a few passes through the
  // informational screens to give folks time to read them.

  for (int i = 0; i < 2; i++) {
    drawWiFiClientInformationFrame();
    delay(SECONDS_BETWEEN_SCREENS);
    drawIotsEndpointInformationFrame();
    delay(SECONDS_BETWEEN_SCREENS);
    drawOauthEndpointInformationFrame();
    delay(SECONDS_BETWEEN_SCREENS);
    drawOauthClientIDInformationFrame();
    delay(SECONDS_BETWEEN_SCREENS);
    drawOauthClientSecretInformationFrame();
    delay(SECONDS_BETWEEN_SCREENS);
    drawSensorIDInformationFrame();
    delay(SECONDS_BETWEEN_SCREENS);
  }

}

void loop() {

  if (restoreBoardToDefault) {
    resetWiFiAndBoard();
  }

  levelPercentage = getFillPercentage();
  deltaValue = fabs(previousLevelPercentage - levelPercentage);
  if (deltaValue > POST_LEVEL_DELTA || (forcePostCount >= MAX_SKIPPED_READINGS)) {
    debug_print("+++ posting: current = %f, previous = %f, delta = %f, forcePostCount = %d\n", levelPercentage, previousLevelPercentage, deltaValue, forcePostCount);
    postLevelPercentage(levelPercentage);
    previousLevelPercentage = levelPercentage;
    forcePostCount = 1;
  } else {
    debug_print("!!!!!! NOT posting: previous = %f, current = %f, delta = %f, forcePostCount = %d\n", previousLevelPercentage, levelPercentage, deltaValue, forcePostCount);
    forcePostCount++;
  }
  // postLevelPercentage(33.48);
  debug_print("Displaying sensor information frame.\n");
  drawSensorInformationFrame();
  delay(POST_INTERVAL_SECONDS);
}
