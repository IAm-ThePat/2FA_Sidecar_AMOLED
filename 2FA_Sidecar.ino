// 2FA Sidecar
// Matt Perkins - Copyright (C) 2023
// Spawned out of the need to often type a lot of two factor authentication
// but still have some security while remaning mostly isolated from the host system.
// See github for 3D models and wiring diagram.
/*

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/


#define NTP_SERVER "ca.pool.ntp.org" //Adjust to your local time perhaps. 
#define TZ "PDT" // Australian Estern time may be needed for clock display in the future. 

// No need to change anything bellow
//

char *mainver = "1.10";

// #include <Adafruit_GFX.h>    // Core graphics library
//#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include "rm67162.h"
#include <TFT_eSPI.h> // Master copy here: https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>
#include <Preferences.h> // perstant storage

// Misc Fonts
// #include "Fonts/FreeSans9pt7b.h"
// #include "Fonts/FreeSans12pt7b.h"
// #include "Fonts/FreeSans18pt7b.h"
// #include "Fonts/FreeMono12pt7b.h"

#include <string>



#include <PinButton.h> // Button Library 

#include <USB.h>
#include <USBHIDKeyboard.h>

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebSrv.h>

#include <lwip/apps/sntp.h>
#include <TOTP-RC6236-generator.hpp>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>



// Init HID so we look like a keyboard
USBHIDKeyboard Keyboard;

// Init our 5 keys
int bargraph_pos;
int bar_width;
int bar_segments;
int updateotp;
long time_x;
PinButton key1(1);
PinButton key2(2);
PinButton key3(3);
PinButton key4(10);
PinButton key5(11);

int keytest = 0;
int sline = 0;
int pinno = 0;
String  in_pin;

// Incorrect Pin Delay
int pindelay = 3000;

AsyncWebServer server(80);

// Setup SSID
String ssid     = "Key-Sidecar";
String password;
String  pin;

String tfa_name_1;
String tfa_seed_1;

String tfa_name_2;
String tfa_seed_2;

String tfa_name_3;
String tfa_seed_3;

String tfa_name_4;
String tfa_seed_4;

String tfa_name_5;
String tfa_seed_5;

// Paramaters wifi
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "password";
const char* PARAM_INPUT_3 = "pin";

// Paramaters 2FA
const char* TFA_INPUT_1 = "tfa_name_1";
const char* TFA_INPUT_2 = "tfa_seed_1";

const char* TFA_INPUT_3 = "tfa_name_2";
const char* TFA_INPUT_4 = "tfa_seed_2";

const char* TFA_INPUT_5 = "tfa_name_3";
const char* TFA_INPUT_6 = "tfa_seed_3";

const char* TFA_INPUT_7 = "tfa_name_4";
const char* TFA_INPUT_8 = "tfa_seed_4";

const char* TFA_INPUT_9 = "tfa_name_5";
const char* TFA_INPUT_10 = "tfa_seed_5";




// Init Screen
//Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
TFT_eSprite spr = TFT_eSprite(&tft);

#define WIDTH  536
#define HEIGHT 240

// Init Preferences
Preferences preferences;

void setup() {
  Serial.begin(115200);

  // // turn on backlite
  // pinMode(TFT_BACKLITE, OUTPUT);
  // digitalWrite(TFT_BACKLITE, HIGH);

  // // turn on the TFT / I2C power supply
  // pinMode(TFT_I2C_POWER, OUTPUT);
  // digitalWrite(TFT_I2C_POWER, HIGH);
  // delay(10);

  // initialize TFT
  rm67162_init();
  lcd_setRotation(3);
  spr.createSprite(WIDTH, HEIGHT);
  spr.setSwapBytes(1);
  spr.fillSprite(TFT_BLACK);
  spr.setFreeFont(&FreeSans12pt7b);

  // Print Bootup
  spr.setCursor(0, 0);
  spr.setTextColor(TFT_WHITE);
  spr.setTextWrap(true);
  spr.printf("\n2FA-Sidecar Ver %s\nBy Matt Perkins (C) 2023\n", mainver);
  spr.printf("Press K1 to enter config/test\n");
  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());

  // Check for key and go to setup /test mode
  int lcount = 0 ;
  while (lcount < 300) {

    key1.update();

    if (key1.isClick()) {
      setup_test();
      ESP.restart();
    }

    spr.printf(".");
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    delay(100);
    lcount++;
  }

  spr.setFreeFont(&FreeSans12pt7b);
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  spr.setCursor(0, 20);
  spr.printf("2FA-Sidecar V%s - startup\n", mainver);
  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());

  preferences.begin("2FA_Sidecar", false);
  //preferences.clear();      // Uncomment if you need to reset

  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");


  // load all pramaters
  tfa_name_1 = preferences.getString("tfa_name_1", "");
  tfa_seed_1 = preferences.getString("tfa_seed_1", "");

  tfa_name_2 = preferences.getString("tfa_name_2", "");
  tfa_seed_2 = preferences.getString("tfa_seed_2", "");

  tfa_name_3 = preferences.getString("tfa_name_3", "");
  tfa_seed_3 = preferences.getString("tfa_seed_3", "");

  tfa_name_4 = preferences.getString("tfa_name_4", "");
  tfa_seed_4 = preferences.getString("tfa_seed_4", "");

  tfa_name_5 = preferences.getString("tfa_name_5", "");
  tfa_seed_5 = preferences.getString("tfa_seed_5", "");

  Serial.print(esp_get_minimum_free_heap_size());
  Serial.print("\n");
  WiFi.begin(ssid, password);
  Serial.print(esp_get_minimum_free_heap_size());
  Serial.print("\n");

  sline = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    spr.printf("Establishing WiFi\n");
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    sline = sline + 1;
    if (sline > 4) {
      spr.fillSprite(TFT_BLACK);
      spr.setTextColor(TFT_WHITE);
      spr.setCursor(0, 20);
      sline = 0 ;
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    }
  }
  spr.setCursor(0, 20);
  spr.print("IP: ");
  spr.println(WiFi.localIP());
  spr.print("Wifi: ");
  spr.print(WiFi.RSSI());

  // start the NTP client
  configTzTime(TZ, NTP_SERVER);
  spr.println();
  spr.printf("NTP started:%s", TZ);
  time_t t = time(NULL);
  spr.printf(":%d", t);
  spr.println();
  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());


  // Check to seee if we have PIN set and ask if we do.
  pin = preferences.getString("pin", "");
  const char* npin = pin.c_str();
  if (strlen(npin) > 3) {
    spr.setCursor(25, 25);
    spr.setFreeFont(&FreeSans12pt7b);
    spr.fillSprite(TFT_WHITE);
    spr.setTextColor(TFT_YELLOW);
    spr.print("PIN?");
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    // read keys for PIN

    while (1) {
      static unsigned long lst = millis();
      if (millis() - lst < 1000) {
        // UPDATE KEYS
        key1.update();
        key2.update();
        key3.update();
        key4.update();
        key5.update();
      }

      lst = millis();

      if (key1.isClick()) {
        pinno = pinno + 1;
        in_pin = in_pin + "1";
        spr.print("*");
        lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());  
      }

      if (key2.isClick()) {
        pinno = pinno + 1;
        in_pin = in_pin + "2";
        spr.print("*");
        lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
      }

      if (key3.isClick()) {
        pinno = pinno + 1;
        in_pin = in_pin + "3";
        spr.print("*");
        lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
      }

      if (key4.isClick()) {
        pinno = pinno + 1;
        in_pin = in_pin + "4";
        spr.print("*");
        lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
      }

      if (key5.isClick()) {
        pinno = pinno + 1;
        in_pin = in_pin + "5";
        spr.print("*");
        lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
      }

      if ( pinno == 4) {
        if (in_pin == npin) {
          spr.println();
          spr.println("Correct.");
          lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
          break;
        } else {
          spr.println();
          spr.print("Incorrect!");
          lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
          delay(pindelay);
          ESP.restart();
        }
      }
    }



  } // end check PIN


  spr.setTextColor(TFT_WHITE);


  // Iniz Keyboard and begin display.
  spr.setFreeFont(&FreeSans9pt7b);

  spr.println("Iniz USB keybaord\n");
  Keyboard.begin();
  USB.begin();
  delay(2000);
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  updateotp = 1;
  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());



}

void loop() {
  static unsigned long lst = millis();
  if (millis() - lst < 1000) {
    // UPDATE KEYS
    key1.update();
    key2.update();
    key3.update();
    key4.update();
    key5.update();
  }

  lst = millis();



  // Update Time.

  time_t t = time(NULL);

  if (t < 1000000) {
    delay(500);
    return;
  };


  bar_width = 310;
  bar_segments = 310/30;
  bargraph_pos = (t % 60);
  if (bargraph_pos > 29) {
    bargraph_pos = bargraph_pos - 30;
  }

  spr.drawRect(4, 149, 312, 12, TFT_WHITE);
  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());

  bargraph_pos = bargraph_pos * bar_segments;
  if (bargraph_pos < 150) {
    spr.fillRect(5,150, bargraph_pos, 10, TFT_GREEN);
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    // spr.fillCircle(5, 155, 4, TFT_GREEN);
  } else if (bargraph_pos < 250) {
    // spr.fillCircle(5, 155, 5, TFT_YELLOW);
    spr.fillRect(5,150, bargraph_pos, 10, TFT_YELLOW);
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
  } else {
    // spr.fillCircle(5, 155, 5, TFT_RED);
    spr.fillRect(5,150, bargraph_pos, 10, TFT_RED);
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
  }

  if (bargraph_pos == 0) {
    updateotp = 1;
  }


  // Display updated OTP per key

  if (updateotp == 1) {
    updateotp = 0;
    spr.setTextColor(TFT_YELLOW);
    spr.setFreeFont(&FreeSans12pt7b);
    spr.fillSprite(TFT_BLACK);

    // Key 1
    if (String * otp1 = TOTP::currentOTP(tfa_seed_1)) {
      spr.setCursor(5, 20);
      spr.setTextColor(TFT_RED);
      spr.setFreeFont(&FreeSans12pt7b);
      spr.print(tfa_name_1);
      spr.setCursor(160, 20);
      spr.setTextColor(TFT_YELLOW);
      spr.setFreeFont(&FreeMonoBold12pt7b);
      spr.println(*otp1);
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    } else {
      spr.setCursor(5, 20);
      spr.setTextColor(TFT_RED);
      spr.print("NO VALID CONFIG");
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
      delay(10000);
      ESP.restart();

    };

    // Key 2
    if (String * otp2 = TOTP::currentOTP(tfa_seed_2)) {
      spr.setCursor(5, 48);
      spr.setTextColor(TFT_RED);
      spr.setFreeFont(&FreeSans12pt7b);
      spr.print(tfa_name_2);
      spr.setCursor(160, 48);
      spr.setTextColor(TFT_YELLOW);
      spr.setFreeFont(&FreeMonoBold12pt7b);
      spr.println(*otp2);
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    };

    // Key 3
    if (String * otp3 = TOTP::currentOTP(tfa_seed_3)) {
      spr.setCursor(5, 76);
      spr.setTextColor(TFT_RED);
      spr.setFreeFont(&FreeSans12pt7b);
      spr.print(tfa_name_3);
      spr.setCursor(160, 76);
      spr.setTextColor(TFT_YELLOW);
      spr.setFreeFont(&FreeMonoBold12pt7b);
      spr.println(*otp3);
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    };

    // Key 4
    if (String * otp4 = TOTP::currentOTP(tfa_seed_4)) {
      spr.setCursor(5, 104);
      spr.setTextColor(TFT_RED);
      spr.setFreeFont(&FreeSans12pt7b);
      spr.print(tfa_name_4);
      spr.setCursor(160, 104);
      spr.setTextColor(TFT_YELLOW);
      spr.setFreeFont(&FreeMonoBold12pt7b);
      spr.println(*otp4);
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    };

    // Key 5
    if (String * otp5 = TOTP::currentOTP(tfa_seed_5)) {
      spr.setCursor(5, 132);
      spr.setTextColor(TFT_RED);
      spr.setFreeFont(&FreeSans12pt7b);
      spr.print(tfa_name_5);
      spr.setCursor(160, 132);
      spr.setTextColor(TFT_YELLOW);
      spr.setFreeFont(&FreeMonoBold12pt7b);
      spr.println(*otp5);
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    };

    // Make up the rest of the second so we dont fliker the screen.
    delay(999);

  }


  // check keypress
  if (key1.isClick()) {
    String * otp1 = TOTP::currentOTP(tfa_seed_1);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Keyboard.println(*otp1);
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (key2.isClick()) {
    String * otp2 = TOTP::currentOTP(tfa_seed_2);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Keyboard.println(*otp2);
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (key3.isClick()) {
    String * otp3 = TOTP::currentOTP(tfa_seed_3);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Keyboard.println(*otp3);
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (key4.isClick()) {
    String * otp4 = TOTP::currentOTP(tfa_seed_4);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Keyboard.println(*otp4);
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (key5.isClick()) {
    String * otp5 = TOTP::currentOTP(tfa_seed_5);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Keyboard.println(*otp5);
    digitalWrite(LED_BUILTIN, LOW);
  }
  // Hold down k5 to restart.
  if (key5.isLongClick()) {
    ESP.restart();
  }



}
