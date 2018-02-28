// #define CLEAR_SAVED_WIFI_SETTINGS 1
#define WRITE_SAMPLE_FILE 1

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

float fill_percentage = -99.9999999;
int sensorPostReturnValue = 202;
#define POST_INTERVAL_SECONDS (20)

// Initialize the WiFiManager.  This library provides a captive portal for configuring
// Wi-Fi connective on the device, as well as on-device configuration and storage of
// random data, like endpoints, passwords, etc.

WiFiManager wifiManager;

// Ultrasonic distance sensor

const int triggerPin = 18;
const int echoPin = 19;

const double speedOfSound = 2.93866995797702; // mm per microsecond
const double heightOfColumn = 573.388650;  // height of the beverage dispenser in mm
double previousLevelPercentage = 0.00;
#define POST_LEVEL_DELTA (0.5)
int forcePostCount = 1;
#define MAX_SKIPPED_READINGS (6)

// File system

#include <FS.h>
#include <SPIFFS.h>

#define CONFIG_FILE "/config.json"
const char *iots_endpoint_key = "iots_endpoint";
#define IOTS_ENDPOINT_SIZE 256
char iots_endpoint_value[IOTS_ENDPOINT_SIZE] = "https://sap-connected-goods-ingestion-api-qa.cfapps.eu10.hana.ondemand.com/ConnectedGoods/v1/DeviceData";
const char *oauth_endpoint_key = "oauth_endpoint";
#define OAUTH_ENDPOINT_SIZE 256
char oauth_endpoint_value[OAUTH_ENDPOINT_SIZE] = "https://cng-qa1.authentication.eu10.hana.ondemand.com/oauth/token";
#define SENSOR_ID_SIZE 128
const char *sensor_id_key = "sensor_id";
char sensor_id_value[SENSOR_ID_SIZE] = "SENSOR_1";
bool needToSaveConfiguration = false;

// Callback that gets triggered when configuration parameters are changed.

void saveConfigCallback() {
  needToSaveConfiguration = true;
  Serial.printf("%s: callback called: needToSaveConfiguration set to true.", __func__);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("%s: Listing directory: %s\n", __func__, dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.printf("%s: Failed to open directory.", __func__);
    return;
  }
  if (!root.isDirectory()) {
    Serial.printf("%s: %s is not a directory.\n", __func__, dirname);
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print(F("  DIR : "));
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print(F("  FILE: "));
      Serial.print(file.name());
      Serial.print(F("  SIZE: "));
      Serial.println(file.size());
    }
    file = root.openNextFile();
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

// String dataServer = F("https://sap-connected-goods-ingestion-api-qa.cfapps.eu10.hana.ondemand.com/ConnectedGoods/v1/DeviceData");  // old one
// String dataServer = F("https://sap-connected-goods-ingestion-api-qa.cfapps.eu10.hana.ondemand.com/ConnectedGoods/v1/DeviceData");
// String authServer = F("https://cng-qa1.authentication.eu10.hana.ondemand.com/oauth/token");

// Root CA certificate obtained from the server

const char* caCert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFSTCCBDGgAwIBAgIQaYeUGdnjYnB0nbvlncZoXjANBgkqhkiG9w0BAQsFADCB\n" \
"vTELMAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQL\n" \
"ExZWZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwOCBWZXJp\n" \
"U2lnbiwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MTgwNgYDVQQDEy9W\n" \
"ZXJpU2lnbiBVbml2ZXJzYWwgUm9vdCBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTAe\n" \
"Fw0xMzA0MDkwMDAwMDBaFw0yMzA0MDgyMzU5NTlaMIGEMQswCQYDVQQGEwJVUzEd\n" \
"MBsGA1UEChMUU3ltYW50ZWMgQ29ycG9yYXRpb24xHzAdBgNVBAsTFlN5bWFudGVj\n" \
"IFRydXN0IE5ldHdvcmsxNTAzBgNVBAMTLFN5bWFudGVjIENsYXNzIDMgU2VjdXJl\n" \
"IFNlcnZlciBTSEEyNTYgU1NMIENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n" \
"CgKCAQEAvjgWUYuA2+oOTezoP1zEfKJd7TuvpdaeEDUs48XlqN6Mhhcm5t4LUUos\n" \
"0PvRFFpy98nduIMcxkaMMSWRDlkXo9ATjJLBr4FUTrxiAp6qpxpX2MqmmXpwVk+Y\n" \
"By5LltBMOVO5YS87dnyOBZ6ZRNEDVHcpK1YqqmHkhC8SFTy914roCR5W8bUUrIqE\n" \
"zq54omAKU34TTBpAcA5SWf9aaC5MRhM7OQmCeAI1SSAIgrOxbIkPbh41JbAsJIPj\n" \
"xVAsukaQRYcNcv9dETjFkXbFLPsFKoKVoVlj49AmWM1nVjq633zS0jvY3hp6d+QM\n" \
"jAvrK8IisL1Vutm5VdEiesYCTj/DNQIDAQABo4IBejCCAXYwEgYDVR0TAQH/BAgw\n" \
"BgEB/wIBADA+BgNVHR8ENzA1MDOgMaAvhi1odHRwOi8vY3JsLndzLnN5bWFudGVj\n" \
"LmNvbS91bml2ZXJzYWwtcm9vdC5jcmwwDgYDVR0PAQH/BAQDAgEGMDcGCCsGAQUF\n" \
"BwEBBCswKTAnBggrBgEFBQcwAYYbaHR0cDovL29jc3Aud3Muc3ltYW50ZWMuY29t\n" \
"MGsGA1UdIARkMGIwYAYKYIZIAYb4RQEHNjBSMCYGCCsGAQUFBwIBFhpodHRwOi8v\n" \
"d3d3LnN5bWF1dGguY29tL2NwczAoBggrBgEFBQcCAjAcGhpodHRwOi8vd3d3LnN5\n" \
"bWF1dGguY29tL3JwYTAqBgNVHREEIzAhpB8wHTEbMBkGA1UEAxMSVmVyaVNpZ25N\n" \
"UEtJLTItMzczMB0GA1UdDgQWBBTbYiD7fQKJfNI7b8fkMmwFUh2tsTAfBgNVHSME\n" \
"GDAWgBS2d/ppSEefUxLVwuoHMnYH0ZcHGTANBgkqhkiG9w0BAQsFAAOCAQEAGcyV\n" \
"4i97SdBIkFP0B7EgRDVwFNVENzHv73DRLUzpLbBTkQFMVOd9m9o6/7fLFK0wD2ka\n" \
"KvC8zTXrSNy5h/3PsVr2Bdo8ZOYr5txzXprYDJvSl7Po+oeVU+GZrYjo+rwJTaLE\n" \
"ahsoOy3DIRXuFPqdmBDrnz7mJCRfehwFu5oxI1h5TOxtGBlNUR8IYb2RBQxanCb8\n" \
"C6UgJb9qGyv3AglyaYMyFMNgW379mjL6tJUOGvk7CaRUR5oMzjKv0SHMf9IG72AO\n" \
"Ym9vgRoXncjLKMziX24serTLR3x0aHtIcQKcIwnzWq5fQi5fK1ktUojljQuzqGH5\n" \
"S5tV1tqxkju/w5v5LA==\n" \
"-----END CERTIFICATE-----\n";

String oauthToken;

// NTP configuration Information

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);
char formatted_time[80];

// Drawing functions for various informational screens.

void drawSetupWiFiFrame() {
  Serial.printf("%s: Displaying frame.\n", __func__);
  display.clear();
  display.drawString(0, LINE_SPACING(0), "Connect to SAP_SensorAP");
  display.drawString(0, LINE_SPACING(1), "with your phone or");
  display.drawString(0, LINE_SPACING(2), "laptop to configure");
  display.drawString(0, LINE_SPACING(3), "Wi-Fi and parameters.");
  display.display();
}

void drawWiFiClientInformationFrame() {
  Serial.printf("%s: Displaying frame.\n", __func__);
  display.clear();
  display.drawString(0, LINE_SPACING(0), "Connected to Wi-Fi network:");
  display.drawStringMaxWidth(0, LINE_SPACING(1), 128, WiFi.SSID());
  String ipString = "IP: " + wifiManager.toStringIp(WiFi.localIP());
  display.drawStringMaxWidth(0, LINE_SPACING(2), 128, ipString);
  display.display();
}

void drawIotsEndpointInformationFrame() {
  Serial.printf("%s: Displaying frame.\n", __func__);
  display.clear();
  String iotsString = "iots: " + String(iots_endpoint_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, iotsString);
  display.display();
}

void drawOauthEndpointInformationFrame() {
  Serial.printf("%s: Displaying frame.\n", __func__);
  display.clear();
  String oauthString = "oauth: " + String(oauth_endpoint_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, oauthString);
  display.display();
}

void drawSensorIDInformationFrame() {
  Serial.printf("%s: Displaying frame.\n", __func__);
  display.clear();
  String sensorIDString = "sensorID: " + String(sensor_id_value);
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, sensorIDString);
  display.display();
}

void drawSensorInformationFrame() {
  Serial.printf("%s: Displaying frame.\n", __func__);
  display.clear();
  display.drawStringMaxWidth(0, LINE_SPACING(0), 128, formatted_time);
  char fill_percentage_string[128];
  String sensorString = "Fill %: " + String(dtostrf(fill_percentage, 4, 2, fill_percentage_string)) + "%";
  display.drawStringMaxWidth(0, LINE_SPACING(1), 128, sensorString);
  String postString = "POST returns: " + String(sensorPostReturnValue);
  display.drawStringMaxWidth(0, LINE_SPACING(2), 128, postString);
  display.display();
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


  http.begin(iots_endpoint_value, caCert);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", oauthToken);
  http.addHeader("cng_devicetype", "Container");
  http.addHeader("cng_messagetype", "Container_Message_Data2");

  // String request = "{\"messages\":[{\"cng_deviceId\":\"" + sensorID + "\",\"timestamp\":\"" + formatted_time + "\",\"flavour\":1,\"fillLevel\":" + levelPercentage + ",\"longitude\":" + longitude + ",\"latitude\":" + latitude + ",\"temperature\":18.1}]}";
  String request = "{\"messages\":[{\"cng_deviceId\":\"" + String(sensor_id_value) + "\",\"timestamp\":\"" + formatted_time + "\",\"filllevel\":" + levelPercentage + ",\"expiryDate\":\"10/10/2022\",\"messagetype\":700,\"filltype\":1,\"fragrance\":\"Ginger\"}]}";

  // log_d("Request: %s\n", request.c_str());
  Serial.printf("%s: Request: %s\n", __func__, request.c_str());
  sensorPostReturnValue = http.POST(request);
  if (sensorPostReturnValue == HTTP_CODE_OK || sensorPostReturnValue == HTTP_CODE_ACCEPTED) {
    response = http.getString();
    Serial.printf("%s: Response: %s, returnCode = %d\n", __func__, response.c_str(), sensorPostReturnValue);
  } else {
    Serial.printf("%s: Error posting sensor data, returnCode = %d\n", __func__, sensorPostReturnValue);
  }
}

// Retrieve an OAuth token from SAP CNG for use with POST requests.

void getAuthToken() {

  String response = "";
  int returnCode = 0;
  HTTPClient http;

  http.begin(oauth_endpoint_value, caCert);

  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String request = F("client_id=sb-sap-connected-goods-qa!t5&client_secret=Eu7gZ7suJG4VRahheB2i3F2E85E=&grant_type=client_credentials&token_format=jwt&response_type=token");

  // Serial.printf("Before calling POST\n");
  returnCode = http.POST(request);
  // Serial.printf("After calling POST\n");
  // returnCode = HTTP_CODE_OK;

  if (returnCode == HTTP_CODE_OK) {
    // Serial.printf("Setting response\n");
    response = http.getString();
    // response = "{\"access_token\":\"abcde\"}";
    // Serial.printf("allocating jsonBuffer<10>\n");
    StaticJsonBuffer<3100> jsonBuffer;
    // StaticJsonBuffer<100> jsonBuffer;
    // Serial.printf("calling parseObject\n");
    JsonObject& root = jsonBuffer.parseObject(response);
    // Serial.printf("retrieving access_token\n");
    const char *token = root["access_token"];
    // Serial.printf("token = %s\n", token);
    // Serial.printf("Creating oauthToken\n");
    oauthToken = "Bearer " + String(token);
    // Serial.printf("oauthToken = %s\n", oauthToken.c_str());
  } else {
    Serial.printf("%s: Error getting auth token, returnCode = %d\n", __func__, returnCode);
  }
}

// Read the distance sensor, determine the distance to the fluid, compute
// the percentage full of the dispenser, and return it.

double getFillPercentage() {

  long duration;
  double distance;
  float percentage;

  // Drop the trigger pin low for 2 microseconds

  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);

  // Send a 10 microsecond burst (high) on the trigger pin

  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  // Read the echo pin, which is the round trip time in microseconds.

  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance in mm.  This is half of the duration
  // divided by the speed of sound in microseconds.

  distance = (duration / 2) / speedOfSound;
  percentage = (100.00 - ((distance / heightOfColumn) * 100.0));

  // Serial.printf("Distance: %f mm, %f%%\n", distance, percentage);

  return(percentage);
}

void setup() {
    Serial.begin(115200);
    delay(5000);

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

    // Uncomment the line below to format the file system.  Best way to remove
    // the configuration file(s) for testing.

    // Serial.println(F("Formatting the SPIFFS."));
    // SPIFFS.format();

    // Mount the filesystem.

    if (SPIFFS.begin()) {

      Serial.printf("%s: Mounted the filesystem.", __func__);


#ifdef WRITE_SAMPLE_FILE

      // Write a test file.

      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json[iots_endpoint_key] = iots_endpoint_value;
      json[oauth_endpoint_key] = oauth_endpoint_value;
      json[sensor_id_key] = sensor_id_value;
      File configFile = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
      if (!configFile) {
        Serial.printf("%s: write test file: Failed to open config file %s for writing.\n", __func__, CONFIG_FILE);
      }

      json.printTo(Serial);
      Serial.println(); // No linefeed in above json.PrintTo(Serial)
      json.printTo(configFile);
      configFile.close();

      // Get the file size and print it.

      configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
      size_t configFileSize = configFile.size();
      Serial.printf("%s: write test file: Wrote config file.  File size is %d bytes.\n", __func__, configFileSize);
      configFile.close();
      Serial.printf("%s: write test file: Root level directory listing:", __func__);
      listDir(SPIFFS, "/", 0);

#endif // WRITE_SAMPLE_FILE

      // Read the config file if it exists.

      if (SPIFFS.exists(CONFIG_FILE)) {

        Serial.printf("%s: Opening the config file %s\n", __func__, CONFIG_FILE);
        File configFile = SPIFFS.open(CONFIG_FILE, "r");  // open for reading only

        if (configFile) {

          Serial.printf("%s: Opened file successfully for reading.", __func__);
          size_t configFileSize = configFile.size();
          Serial.printf("%s: File size is %d bytes.\n", __func__, configFileSize);

          // Allocate a buffer into which to read the whole file.

          char *configBuffer = (char *)malloc(configFileSize * sizeof(char));

          // Read the file into the buffer.

          int bytesRead = configFile.readBytes(configBuffer, configFileSize);
          Serial.printf("%s: Read %d bytes from the config file\n", __func__, bytesRead);
          Serial.printf("%s: String length of contents: %d\n", __func__, strlen(configBuffer));
          Serial.printf("%s: Contents: %s\n", __func__, configBuffer);
          configBuffer[bytesRead] = '\0';
          Serial.printf("%s: Contents (with null): %s\n", __func__, configBuffer);

          // Parse the buffer.

          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(configBuffer);
          json.printTo(Serial);
          if (json.success()) {
            Serial.printf("\n%s: Successfully parsed json", __func__);
            strcpy(iots_endpoint_value, json[iots_endpoint_key]);
            Serial.printf("%s: %s = %s\n", __func__, iots_endpoint_key, iots_endpoint_value);
            strcpy(oauth_endpoint_value, json[oauth_endpoint_key]);
            Serial.printf("%s: %s = %s\n", __func__, oauth_endpoint_key, oauth_endpoint_value);
            strcpy(sensor_id_value, json[sensor_id_key]);
            Serial.printf("%s: %s = %s\n", __func__, sensor_id_key, sensor_id_value);
          } else {
            Serial.printf("%s: Failed to load json config.", __func__);
          }
        } // Read the config file

      } else {
        Serial.printf("%s: Config file %s does not exist.\n", __func__, CONFIG_FILE);
      }

    } else {
      Serial.printf("%s: Failed to mount SPIFFS!!!!!!", __func__);
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
    WiFiManagerParameter custom_sensor_id_value("Sensor ID", "Custom Sensor ID", sensor_id_value, SENSOR_ID_SIZE);
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&custom_iots_endpoint_value);
    wifiManager.addParameter(&custom_oauth_endpoint_value);
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

    Serial.printf("%s: Connected to SSID %s, IP %s\n", __func__, WiFi.SSID().c_str(), wifiManager.toStringIp(WiFi.localIP()).c_str());

    // Now that we're connected, display the new Wi-Fi and configuration information.

    // Get the custom parameters from the configuration page.

    strcpy(iots_endpoint_value, custom_iots_endpoint_value.getValue());
    strcpy(oauth_endpoint_value, custom_oauth_endpoint_value.getValue());
    strcpy(sensor_id_value, custom_sensor_id_value.getValue());

    // If we changed any paramater configuration, save it.

    if (needToSaveConfiguration) {
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json[iots_endpoint_key] = iots_endpoint_value;
      json[oauth_endpoint_key] = oauth_endpoint_value;
      json[sensor_id_key] = sensor_id_value;
      File configFile = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
      if (!configFile) {
        Serial.printf("%s: Failed to open config file %s for writing.\n", __func__, CONFIG_FILE);
      }

      json.printTo(Serial);
      Serial.println(); // No linefeed in above json.PrintTo(Serial)
      json.printTo(configFile);
      configFile.close();

      // Get the file size and print it.

      configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
      size_t configFileSize = configFile.size();
      Serial.printf("%s: Wrote conig file.  File size is %d bytes.\n", __func__, configFileSize);
      configFile.close();
      Serial.printf("%s: Root level directory listing:", __func__);
      listDir(SPIFFS, "/", 0);

    }

    // If you want to clear the saved SSIDs/passwords, uncomment the following
    // two lines.  This is specific to the ESP32 and is a current limitation.
    // Hopefully it will be fixed in the future.  Note that this only works
    // if you have successfully connected to a Wi-Fi network.

#ifdef CLEAR_SAVED_WIFI_SETTINGS
    Serial.printf("%s: Clearing the saved SSID/password information.", __func__);
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
    drawSensorIDInformationFrame();
    delay(SECONDS_BETWEEN_SCREENS);
  }

}

void loop() {
    getAuthToken();
    // Serial.printf("oauthToken: %s\n", oauthToken.c_str());

    double levelPercentage = getFillPercentage();
    double deltaValue = fabs(previousLevelPercentage - levelPercentage);
    if (deltaValue > POST_LEVEL_DELTA || (forcePostCount >= MAX_SKIPPED_READINGS)) {
      Serial.printf("%s: +++ posting: current = %f, previous = %f, delta = %f, forcePostCount = %d\n", __func__, levelPercentage, previousLevelPercentage, deltaValue, forcePostCount);
      postLevelPercentage(levelPercentage);
      previousLevelPercentage = levelPercentage;
      forcePostCount = 1;
    } else {
      Serial.printf("%s: !!!!!! NOT posting: previous = %f, current = %f, delta = %f, forcePostCount = %d\n", __func__, previousLevelPercentage, levelPercentage, deltaValue, forcePostCount);
      forcePostCount++;
    }
    // postLevelPercentage(33.48);
  Serial.printf("%s: Displaying sensor information frame.\n", __func__);
  drawSensorInformationFrame();
  delay(POST_INTERVAL_SECONDS);
}
