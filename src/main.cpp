// #define CLEAR_SAVED_WIFI_SETTINGS 1
// #define WRITE_SAMPLE_FILE 1

#include <Arduino.h>

#include <ArduinoJson.h>

// Wi-Fi and WiFiManager

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

//needed for library
#include <DNSServer.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif
#include <WiFiManager.h>

#define AP_SSID "SAP_SensorAP"

// File system

#include <FS.h>
#include <SPIFFS.h>

#define CONFIG_FILE "/config.json"
const char *iots_endpoint_key = "iots_endpoint";
#define IOTS_ENDPOINT_SIZE 256
char iots_endpoint_value[IOTS_ENDPOINT_SIZE] = "http://iots.sap.com/foo/v1/";
const char *oauth_endpoint_key = "oauth_endpoint";
#define OAUTH_ENDPOINT_SIZE 256
char oauth_endpoint_value[OAUTH_ENDPOINT_SIZE] = "http://oauth.sap.com/bar/baz/bax/";
#define SENSOR_ID_SIZE 128
const char *sensor_id_key = "sensor_id";
char sensor_id_value[SENSOR_ID_SIZE] = "SENSOR_1";
bool needToSaveConfiguration = false;

// Callback that gets triggered when configuration parameters are changed.

void saveConfigCallback() {
  needToSaveConfiguration = true;
  Serial.println(F("saveConfigCallback called: needToSaveConfiguration set to true."));
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

SSD1306  display(0x3c, 4, 15);

int fontHeight = -1;
#define LINE_SPACING(X) (fontHeight * X)
#define MAX_DISPLAY_STRING_LENGTH (25)

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("listDir: Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println(F("listDir: Failed to open directory."));
    return;
  }
  if (!root.isDirectory()) {
    Serial.printf("listDir: %s is not a directory.\n", dirname);
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

void setup() {
    Serial.begin(115200);
    delay(5000);

    // Retrieve custom parameters.  Do this before anything else, since we'll
    // need them to populate the WiFiManager portal page.

    // Uncomment the line below to format the file system.  Best way to remove
    // the configuration file(s) for testing.

    // Serial.println(F("Formatting the SPIFFS"));
    // SPIFFS.format();

    // Mount the filesystem.

    if (SPIFFS.begin()) {

      Serial.println(F("setup: Mounted the filesystem."));


#ifdef WRITE_SAMPLE_FILE

      // Write a test file.

      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json[iots_endpoint_key] = iots_endpoint_value;
      json[oauth_endpoint_key] = oauth_endpoint_value;
      json[sensor_id_key] = sensor_id_value;
      File configFile = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
      if (!configFile) {
        Serial.printf("setup: write test file: Failed to open config file %s for writing.\n", CONFIG_FILE);
      }

      json.printTo(Serial);
      Serial.println(); // No linefeed in above json.PrintTo(Serial)
      json.printTo(configFile);
      configFile.close();

      // Get the file size and print it.

      configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
      size_t configFileSize = configFile.size();
      Serial.printf("setup: write test file: Wrote config file.  File size is %d bytes.\n", configFileSize);
      configFile.close();
      Serial.println(F("setup: write test file: Root level directory listing:"));
      listDir(SPIFFS, "/", 0);

#endif // WRITE_SAMPLE_FILE

      // Read the config file if it exists.

      if (SPIFFS.exists(CONFIG_FILE)) {

        Serial.printf("setup: Opening the config file %s\n", CONFIG_FILE);
        File configFile = SPIFFS.open(CONFIG_FILE, "r");  // open for reading only

        if (configFile) {

          Serial.println(F("setup: Opened file successfully for reading."));
          size_t configFileSize = configFile.size();
          Serial.printf("setup: File size is %d bytes.\n", configFileSize);

          // Allocate a buffer into which to read the whole file.

          char *configBuffer = (char *)malloc(configFileSize * sizeof(char));

          // Read the file into the buffer.

          int bytesRead = configFile.readBytes(configBuffer, configFileSize);
          Serial.printf("setup: Read %d bytes from the config file\n", bytesRead);
          Serial.printf("setup: String length of contents: %d\n", strlen(configBuffer));
          Serial.printf("setup: Contents: %s\n", configBuffer);
          configBuffer[bytesRead] = '\0';
          Serial.printf("setup: Contents (with null): %s\n", configBuffer);

          // Parse the buffer.

          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(configBuffer);
          json.printTo(Serial);
          if (json.success()) {
            Serial.println("\nsetup: Successfully parsed json");
            strcpy(iots_endpoint_value, json[iots_endpoint_key]);
            Serial.printf("setup: %s = %s\n", iots_endpoint_key, iots_endpoint_value);
            strcpy(oauth_endpoint_value, json[oauth_endpoint_key]);
            Serial.printf("setup: %s = %s\n", oauth_endpoint_key, oauth_endpoint_value);
            strcpy(sensor_id_value, json[sensor_id_key]);
            Serial.printf("setup: %s = %s\n", sensor_id_key, sensor_id_value);
          } else {
            Serial.println(F("setup: Failed to load json config."));
          }
        } // Read the config file

      } else {
        Serial.printf("setup: Config file %s does not exist.\n", CONFIG_FILE);
      }

    } else {
      Serial.println(F("setup: Failed to mount SPIFFS!!!!!!"));
    }

    // Set up the display.

    display.init();
    display.clear();

    // Change the screen orientation and choose a small, good looking font.

    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    fontHeight = ArialMT_Plain_10[1] - 4; // Tighten things up a bit.

    // Initialize the WiFiManager.  This library provides a captive portal for configuring
    // Wi-Fi connective on the device, as well as on-device configuration and storage of
    // random data, like endpoints, passwords, etc.

    WiFiManager wifiManager;

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

    display.drawString(0, LINE_SPACING(0), "Connect to SAP_SensorAP");
    display.drawString(0, LINE_SPACING(1), "with your phone or");
    display.drawString(0, LINE_SPACING(2), "laptop to configure");
    display.drawString(0, LINE_SPACING(3), "Wi-Fi and parameters.");
    display.display();

    wifiManager.autoConnect(AP_SSID);

    // Use this variant to have an SSID auto generated.  The same rules as above
    // apply.

    // wifiManager.autoConnect();

    // We have connected to the Wi-Fi network.  Print some information and carry
    // on.

    Serial.printf("setup: Connected to SSID %s, IP %s\n", WiFi.SSID().c_str(), wifiManager.toStringIp(WiFi.localIP()).c_str());

    display.clear();
    display.drawString(0, LINE_SPACING(0), "Connected to Wi-Fi network!");
    display.drawString(0, LINE_SPACING(1), WiFi.SSID());
    display.drawString(0, LINE_SPACING(2), wifiManager.toStringIp(WiFi.localIP()));

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
        Serial.printf("setup: Failed to open config file %s for writing.\n", CONFIG_FILE);
      }

      json.printTo(Serial);
      Serial.println(); // No linefeed in above json.PrintTo(Serial)
      json.printTo(configFile);
      configFile.close();

      // Get the file size and print it.

      configFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
      size_t configFileSize = configFile.size();
      Serial.printf("setup: Wrote conig file.  File size is %d bytes.\n", configFileSize);
      configFile.close();
      Serial.println(F("setup: Root level directory listing:"));
      listDir(SPIFFS, "/", 0);

    }

    // If you want to clear the saved SSIDs/passwords, uncomment the following
    // two lines.  This is specific to the ESP32 and is a current limitation.
    // Hopefully it will be fixed in the future.  Note that this only works
    // if you have successfully connected to a Wi-Fi network.

#ifdef CLEAR_SAVED_WIFI_SETTINGS
    Serial.println(F("Clearing the saved SSID/password information."));
    WiFi.disconnect(true);
#endif // CLEAR_SAVED_WIFI_SETTINGS

    // Display the new custom parameters.  Just guessing at the offsets for
    // the keys.

    display.drawString(0, LINE_SPACING(3), "iots: ");
    display.drawString(20, LINE_SPACING(3), iots_endpoint_value);
    display.drawString(0, LINE_SPACING(4), "oauth: ");
    display.drawString(30, LINE_SPACING(4), oauth_endpoint_value);
    display.drawString(0, LINE_SPACING(5), "sensor: ");
    display.drawString(35, LINE_SPACING(5), sensor_id_value);
    display.display();

}

void loop() {
}
