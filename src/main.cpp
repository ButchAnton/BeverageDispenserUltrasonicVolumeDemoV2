#include <Arduino.h>

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

void setup() {
    Serial.begin(115200);
    delay(5000);

    // Set up the display.

    display.init();
    display.clear();

    // Change the screen orientation and choose a small, good looking font.

    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    fontHeight = ArialMT_Plain_10[1];

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

    Serial.printf("Connected to SSID %s, IP %s\n", WiFi.SSID().c_str(), wifiManager.toStringIp(WiFi.localIP()).c_str());

    display.clear();
    // display.display();
    display.drawString(0, LINE_SPACING(0), "Connected to Wi-Fi network!");
    display.drawString(0, LINE_SPACING(1), WiFi.SSID());
    display.drawString(0, LINE_SPACING(2), wifiManager.toStringIp(WiFi.localIP()));
    display.display();


    // If you want to clear the saved SSIDs/passwords, uncomment the following
    // two lines.  This is specific to the ESP32 and is a current limitation.
    // Hopefully it will be fixed in the future.  Note that this only works
    // if you have successfully connected to a Wi-Fi network.

    // Serial.printf("Clearing the saved SSID/password information.\n");
    // WiFi.disconnect(true);
}

void loop() {
}
