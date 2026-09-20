/*
   ____  __  __  ____  _  _  _____       ___  _____  ____  _  _
  (  _ \(  )(  )(_  _)( \( )(  _  )___  / __)(  _  )(_  _)( \( )
   )(_) ))(__)(  _)(_  )  (  )(_)((___)( (__  )(_)(  _)(_  )  (
  (____/(______)(____)(_)\_)(_____)     \___)(_____)(____)(_)\_)
  Official code for all ESP8266/32 boards            version 4.3
  Main .ino file

  The Duino-Coin Team & Community 2019-2024 Â© MIT Licensed
  https://duinocoin.com
  https://github.com/revoxhere/duino-coin

  If you don't know where to start, visit official website and navigate to
  the Getting Started page. Have fun mining!

  To edit the variables (username, WiFi settings, etc.) use the Settings.h tab!
*/

/* If optimizations cause problems, change them to -O0 (the default) */
#pragma GCC optimize("-Ofast")

/* If during compilation the line below causes a
  "fatal error: arduinoJson.h: No such file or directory"
  message to occur; it means that you do NOT have the
  ArduinoJSON library installed. To install it,
  go to the below link and follow the instructions:
  https://github.com/revoxhere/duino-coin/issues/832 */
#include <ArduinoJson.h>

unsigned long lastDisplayUpdate = 0;

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
    #include <ESP8266HTTPClient.h>
    #include <ESP8266WebServer.h>
#else
    #include <ESPmDNS.h>
    #include <WiFi.h>
    #include <HTTPClient.h>
    #include <WebServer.h>
#endif

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <Ticker.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "MiningJob.h"
#include "Settings.h"

#if defined(DISPLAY_NMTV154)
  #include <Preferences.h>
  static uint32_t nm_temp_interval_seconds = DIAG_DEFAULT_TEMP_INTERVAL;
  static volatile float nm_cpu_temp = 0.0f;
  static volatile float nm_cpu_temp_max = 0.0f;
  static unsigned long nm_temp_last_read = 0;
  static void nm_diag_load() {
    Preferences p; p.begin("nm_diag", true);
    nm_temp_interval_seconds = constrain((uint32_t)p.getUInt("tempint", DIAG_DEFAULT_TEMP_INTERVAL), 1UL, 3600UL);
    p.end();
  }
  static void nm_temp_update(bool force=false) {
    #if WEB_DIAG_CPU_TEMP
      unsigned long now=millis();
      if (!force && now-nm_temp_last_read < nm_temp_interval_seconds*1000UL) return;
      nm_temp_last_read=now;
      float t=temperatureRead();
      if (isfinite(t)) { nm_cpu_temp=t; if (nm_cpu_temp_max==0.0f || t>nm_cpu_temp_max) nm_cpu_temp_max=t; }
    #else
      (void)force;
    #endif
  }
#endif

#if defined(DISPLAY_NMTV154)
  #include <SPI.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_ST7789.h>
  #include <Preferences.h>
  #include "pins.h"

  // DISPLAY_NMTV154 ist jetzt bereits durch Settings.h bekannt.
  Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

  // V12: Backlight per PWM, LOW-aktiv.
  // ESP32 Arduino Core 3.x: LEDC wird direkt am Pin angehaengt.
  // Genau dieselbe API wie im funktionierenden Wetter-Projekt.
  static const uint16_t NM_BL_FREQ = 1000;
  static const uint8_t NM_BL_RESOLUTION = 8;
  static uint8_t nm_brightness_percent = 100;
  static bool nm_backlight_pwm_active = false;
  static bool nm_display_light_on = true;

  // Wallet-Refresh: Standard 300 Sekunden (5 Minuten), dauerhaft in NVS gespeichert.
  static uint32_t nm_wallet_interval_seconds = 300;
  static unsigned long nm_wallet_last_attempt = 0;

  static void nm_load_wallet_interval() {
    Preferences p;
    p.begin("nm_display", true);
    nm_wallet_interval_seconds = p.getUInt("wallet_sec", 300);
    p.end();
    if (nm_wallet_interval_seconds < 60) nm_wallet_interval_seconds = 60;
    if (nm_wallet_interval_seconds > 86400) nm_wallet_interval_seconds = 86400;
  }

  static void nm_save_wallet_interval(uint32_t seconds) {
    seconds = constrain(seconds, 60UL, 86400UL);
    nm_wallet_interval_seconds = seconds;
    Preferences p;
    p.begin("nm_display", false);
    p.putUInt("wallet_sec", nm_wallet_interval_seconds);
    p.end();
  }

  static void nm_apply_brightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    nm_brightness_percent = percent;

    // Backlight ist LOW-aktiv:
    // 100% => Duty 0, 0% => Duty 255.
    const uint8_t pwm = 255 - (uint8_t)((percent * 255UL) / 100UL);
    ledcWrite(TFT_BACKLIGHT, pwm);
    nm_display_light_on = (percent > 0);
  }

  static void nm_backlight_setup() {
    // Arduino-ESP32 3.x API:
    // ledcAttach(pin, frequency, resolution)
    nm_backlight_pwm_active =
        ledcAttach(TFT_BACKLIGHT, NM_BL_FREQ, NM_BL_RESOLUTION);

    Preferences p;
    p.begin("nm_display", true);
    nm_brightness_percent = p.getUChar("brightness", 100);
    p.end();

    nm_apply_brightness(nm_brightness_percent);
  }

  static void nm_save_brightness(uint8_t percent) {
    nm_apply_brightness(percent);
    Preferences p;
    p.begin("nm_display", false);
    p.putUChar("brightness", nm_brightness_percent);
    p.end();
  }
#endif

#ifdef USE_LAN
  #include <ETH.h>
#endif

#if defined(WEB_DASHBOARD)
  #include "Dashboard.h"
#endif

#if defined(DISPLAY_NMTV154)
  #include "NMDisplay.h"
#elif defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2)
  #include "DisplayHal.h"
#endif

#if !defined(ESP8266) && defined(DISABLE_BROWNOUT)
    #include "soc/soc.h"
    #include "soc/rtc_cntl_reg.h"
#endif

// Auto adjust physical core count
// (ESP32-S2/C3 have 1 core, ESP32 has 2 cores, ESP8266 has 1 core)
#if defined(ESP8266)
    #define CORE 1
    typedef ESP8266WebServer WebServer;
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
    #define CORE 1
#elif defined(CONFIG_FREERTOS_UNICORE)
    #define CORE 1
#else
    #define CORE 2
    // Install TridentTD_EasyFreeRTOS32 if you get an error
    #include <TridentTD_EasyFreeRTOS32.h>

    void Task1Code( void * parameter );
    void Task2Code( void * parameter );
    TaskHandle_t Task1;
    TaskHandle_t Task2;
#endif

#if defined(WEB_DASHBOARD)
    WebServer server(80);
#endif 

// Single-Core-Servicehook: wird auch waehrend langer Hash-Schleifen aufgerufen.
void duco_background_events() {
  ArduinoOTA.handle();
  #if defined(WEB_DASHBOARD)
    server.handleClient();
  #endif
  #if defined(DISPLAY_NMTV154)
    update_display_clock();

    // C2 ist Single-Core: system_events_func() läuft hier nicht.
    // Deshalb muss der Wallet-Timer in diesem regelmäßig aufgerufenen Hook sitzen.
    const unsigned long walletNow = millis();
    const unsigned long walletIntervalMs = nm_wallet_interval_seconds * 1000UL;
    if (walletNow - nm_wallet_last_attempt >= walletIntervalMs) {
      nm_wallet_last_attempt = walletNow;
      update_wallet_data();
    }
  #endif
  yield();
}

#if defined(CAPTIVE_PORTAL)
  #include <FS.h> // This needs to be first, or it all crashes and burns...
  #include <WiFiManager.h>
  #include <Preferences.h>
  char duco_username[40];
  char duco_password[40];
  char duco_rigid[24];
  WiFiManager wifiManager;
  Preferences preferences;
  WiFiManagerParameter custom_duco_username("duco_usr", "Duino-Coin username", duco_username, 40);
  WiFiManagerParameter custom_duco_password("duco_pwd", "Duino-Coin mining key (if enabled in the wallet)", duco_password, 40);
  WiFiManagerParameter custom_duco_rigid("duco_rig", "Custom miner identifier (optional)", duco_rigid, 24);
  
  void saveConfigCallback() {
    preferences.begin("duino_config", false);
    preferences.putString("duco_username", custom_duco_username.getValue());
    preferences.putString("duco_password", custom_duco_password.getValue());
    preferences.putString("duco_rigid", custom_duco_rigid.getValue());
    preferences.end();
    RestartESP("Settings saved");
  }

  void reset_settings() {
    server.send(200, "text/html", "Settings have been erased. Please redo the configuration by connecting to the WiFi network that will be created");
    delay(500);
    wifiManager.resetSettings();
    RestartESP("Manual settings reset");
  }

  void saveParamCallback(){
    Serial.println("[CALLBACK] saveParamCallback fired");
    Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
  }

  String getParam(String name){
    //read parameter from server, for customhmtl input
    String value;
    if(wifiManager.server->hasArg(name)) {
      value = wifiManager.server->arg(name);
    }
    return value;
  }
#endif

void RestartESP(String msg) {
  #if defined(SERIAL_PRINTING)
    Serial.println(msg);
    Serial.println("Restarting ESP...");
  #endif

  #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
    display_info("Restarting ESP...");
  #endif

  #if defined(ESP8266)
    ESP.reset();
  #else
    ESP.restart();
    abort();
  #endif
}

#if defined(BLUSHYBOX)
    Ticker blinker;
    bool lastLedState = false;
    void changeState() {
      analogWrite(LED_BUILTIN, lastLedState ? 255 : 0);
      lastLedState = !lastLedState;
    }
#endif

#if defined(ESP8266)
    // WDT Loop 
    // See lwdtcb() and lwdtFeed() below
    Ticker lwdTimer;
    
    unsigned long lwdCurrentMillis = 0;
    unsigned long lwdTimeOutMillis = LWD_TIMEOUT;

    void ICACHE_RAM_ATTR lwdtcb(void) {
      if ((millis() - lwdCurrentMillis > LWD_TIMEOUT) || (lwdTimeOutMillis - lwdCurrentMillis != LWD_TIMEOUT))
        RestartESP("Loop WDT Failed!");
    }
    
    void lwdtFeed(void) {
      lwdCurrentMillis = millis();
      lwdTimeOutMillis = lwdCurrentMillis + LWD_TIMEOUT;
    }
#else
    void lwdtFeed(void) {
      yield();
    }
#endif

namespace {
    MiningConfig *configuration = new MiningConfig(
        DUCO_USER,
        RIG_IDENTIFIER,
        MINER_KEY
    );

    #if defined(ESP32) && CORE == 2
      EasyMutex mutexClientData, mutexConnectToServer;
    #endif

    #ifdef USE_LAN
      static bool eth_connected = false;
    #endif

    void UpdateHostPort(String input) {
        // Thanks @ricaun for the code
        DynamicJsonDocument doc(256);
        deserializeJson(doc, input);
        const char *name = doc["name"];

        configuration->host = doc["ip"].as<String>().c_str();
        configuration->port = doc["port"].as<int>();
        node_id = String(name);

        #if defined(SERIAL_PRINTING)
          Serial.println("Poolpicker selected the best mining node: " + node_id);
        #endif

        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
          display_info(node_id);
        #endif
    }

    void VerifyWifi() {
      #ifdef USE_LAN
        while ((!eth_connected) || (ETH.localIP() == IPAddress(0, 0, 0, 0))) {
          #if defined(SERIAL_PRINTING)
            Serial.println("Ethernet connection lost. Reconnect..." );
          #endif
          SetupWifi();
        }
      #else
        while (WiFi.status() != WL_CONNECTED 
                || WiFi.localIP() == IPAddress(0, 0, 0, 0)
                || WiFi.localIP() == IPAddress(192, 168, 4, 2) 
                || WiFi.localIP() == IPAddress(192, 168, 4, 3)) {
            #if defined(SERIAL_PRINTING)
              Serial.println("WiFi reconnecting...");
            #endif
            WiFi.disconnect();
            delay(500);
            WiFi.reconnect();
            delay(500);
        }
      #endif
    }

    String httpGetString(String URL) {
        String payload = "";
        
        WiFiClient client;
        HTTPClient http;

        if (!http.begin(client, URL)) {
            #if defined(SERIAL_PRINTING)
              Serial.println("HTTP begin failed");
            #endif
            return payload;
        }
        http.addHeader("Accept", "*/*");
        
        int httpCode = http.GET();
        #if defined(SERIAL_PRINTING)
            Serial.printf("HTTP Response code: %d\n", httpCode);
        #endif

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = http.getString();
        } else {
            #if defined(SERIAL_PRINTING)
               Serial.printf("Error fetching node from poolpicker: %s\n", http.errorToString(httpCode).c_str());
               VerifyWifi();
            #endif
            #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
              display_info(http.errorToString(httpCode));
            #endif
        }
        http.end();
        return payload;
    }

    void SelectNode() {
        String input = "";
        int waitTime = 1;
        int poolIndex = 0;

        while (input == "") {
            #if defined(SERIAL_PRINTING)
              Serial.println("Fetching mining node from the poolpicker in " + String(waitTime) + "s");
            #endif
            delay(waitTime * 1000);
            
            input = httpGetString("http://server.duinocoin.com/getPool");
            
            // Increase wait time till a maximum of 32 seconds
            // (addresses: Limit connection requests on failure in ESP boards #1041)
            waitTime *= 2;
            if (waitTime > 32) 
                RestartESP("Node fetch unavailable");
        }

        UpdateHostPort(input);
      }

    #ifdef USE_LAN
        void WiFiEvent(WiFiEvent_t event) {
            switch (event) {
              case ARDUINO_EVENT_ETH_START:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Started");
                #endif
                // The hostname must be set after the interface is started, but needs
                // to be set before DHCP, so set it from the event handler thread.
                ETH.setHostname("esp32-ethernet");
                break;
            case ARDUINO_EVENT_ETH_CONNECTED:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Connected");
                #endif
                break;
            case ARDUINO_EVENT_ETH_GOT_IP:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Got IP");
                #endif
                eth_connected = true;
                break;
            case ARDUINO_EVENT_ETH_DISCONNECTED:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Disconnected");
                #endif
                eth_connected = false;
                break;
            case ARDUINO_EVENT_ETH_STOP:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Stopped");
                #endif
                eth_connected = false;
                break;
            default:
                break;
            }
        }
    #endif

    void SetupWifi() {
      #ifdef USE_LAN
        #if defined(SERIAL_PRINTING)
            Serial.println("Connecting to Ethernet...");
        #endif
        WiFi.onEvent(WiFiEvent);  // Will call WiFiEvent() from another thread.
        ETH.begin();
        
        while (!eth_connected) {
            delay(500);
            #if defined(SERIAL_PRINTING)
                Serial.print(".");
            #endif
        }

        #if defined(SERIAL_PRINTING)
            Serial.println("\n\nSuccessfully connected to Ethernet");
            Serial.println("Local IP address: " + ETH.localIP().toString());
            Serial.println("Rig name: " + String(RIG_IDENTIFIER));
            Serial.println();
        #endif

      #else
        #if defined(SERIAL_PRINTING)
            Serial.println("Connecting to: " + String(SSID));
        #endif
        
        WiFi.begin(SSID, PASSWORD);
        while(WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(100);
        }
        VerifyWifi();
        
        #if !defined(ESP8266)
              WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), DNS_SERVER);
        #endif

        #if defined(SERIAL_PRINTING)
            Serial.println("\n\nSuccessfully connected to WiFi");
            Serial.println("Rig name: " + String(RIG_IDENTIFIER));
            Serial.println("Local IP address: " + WiFi.localIP().toString());
            Serial.println("Gateway: " + WiFi.gatewayIP().toString());
            Serial.println("DNS: " + WiFi.dnsIP().toString());
            Serial.println();
        #endif

      #endif

      #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
          display_info("Waiting for node...");
      #endif
      SelectNode();
    }

    void SetupOTA() {
        // Prepare OTA handler
        ArduinoOTA.onStart([]()
                           { 
                             #if defined(SERIAL_PRINTING)
                               Serial.println("Start"); 
                             #endif
                           });
        ArduinoOTA.onEnd([]()
                         { 
                            #if defined(SERIAL_PRINTING)
                              Serial.println("\nEnd"); 
                            #endif
                         });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                              { 
                                 #if defined(SERIAL_PRINTING)
                                   Serial.printf("Progress: %u%%\r", (progress / (total / 100))); 
                                 #endif
                              });
        ArduinoOTA.onError([](ota_error_t error)
                           {
                                Serial.printf("Error[%u]: ", error);
                                #if defined(SERIAL_PRINTING)
                                  if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                                  else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                                  else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                                  else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                                  else if (error == OTA_END_ERROR) Serial.println("End Failed");
                                #endif
                          });

        ArduinoOTA.setHostname(RIG_IDENTIFIER); // Give port a name
        ArduinoOTA.begin();
    }

    #if defined(WEB_DASHBOARD)
        void dashboard() {
             #if defined(SERIAL_PRINTING)
               Serial.println("Handling HTTP client");
             #endif
             String s = WEBSITE;
             #ifdef USE_LAN
              s.replace("@@IP_ADDR@@", ETH.localIP().toString());
             #else
              s.replace("@@IP_ADDR@@", WiFi.localIP().toString());
             #endif
  
             s.replace("@@HASHRATE@@", String((hashrate+hashrate_core_two) / 1000));
             s.replace("@@DIFF@@", String(difficulty / 100));
             s.replace("@@SHARES@@", String(rejected_share_count) + "/" + String(accepted_share_count));
             s.replace("@@CRASHLOG@@", crashlog_html());
             s.replace("@@NODE@@", String(node_id));
             #if defined(DISPLAY_NMTV154)
               s.replace("@@BRIGHTNESS@@", String(nm_brightness_percent));
               s.replace("@@WALLET_BALANCE@@", duco_wallet_valid ? duco_balance_raw : String("--"));
               s.replace("@@WALLET_USD@@", duco_wallet_valid ? nm_wallet_usd_text() : String("--"));
               s.replace("@@ACTIVE_DEVICES@@", String(duco_active_devices));
               s.replace("@@ACTIVE_THREADS@@", String(duco_active_threads));
               s.replace("@@TOTAL_HASHRATE@@", nm_format_hashrate(duco_total_hashrate));
               s.replace("@@TOTAL_ACCEPTED@@", String(duco_total_accepted));
               s.replace("@@TOTAL_REJECTED@@", String(duco_total_rejected));
               s.replace("@@MINER_ROWS@@", duco_miner_rows_html);
               s.replace("@@WALLET_INTERVAL@@", String(nm_wallet_interval_seconds));
               s.replace("@@WALLET_LAST_UPDATE@@", wallet_last_update_text());
             #else
               s.replace("@@BRIGHTNESS@@", "100");
               s.replace("@@WALLET_BALANCE@@", "--");
               s.replace("@@WALLET_USD@@", "--");
               s.replace("@@ACTIVE_DEVICES@@", "0");
               s.replace("@@ACTIVE_THREADS@@", "0");
               s.replace("@@TOTAL_HASHRATE@@", "0.000 H/s");
               s.replace("@@TOTAL_ACCEPTED@@", "0");
               s.replace("@@TOTAL_REJECTED@@", "0");
               s.replace("@@MINER_ROWS@@", "<tr><td colspan='7'>--</td></tr>");
               s.replace("@@WALLET_INTERVAL@@", "300");
               s.replace("@@WALLET_LAST_UPDATE@@", "--");
             #endif
             
             #if defined(ESP8266)
                 s.replace("@@DEVICE@@", "ESP8266");
             #elif defined(CONFIG_FREERTOS_UNICORE)
                 #if defined(CONFIG_IDF_TARGET_ESP32C2)
                 s.replace("@@DEVICE@@", "ESP32-C2 RISC-V");
               #else
                 s.replace("@@DEVICE@@", "ESP32-S2/C3");
               #endif
             #else
                 s.replace("@@DEVICE@@", "ESP32");
             #endif
             
             s.replace("@@ID@@", String(RIG_IDENTIFIER));
             s.replace("@@MEMORY@@", String(ESP.getFreeHeap()));
             s.replace("@@VERSION@@", String(SOFTWARE_VERSION));
             #if WEB_DIAG_CPU_TEMP
               s.replace("@@CPU_TEMP_FIELDS@@", "<div class='stat'><div class='value' id='cpuTemp'>-- &deg;C</div><div class='label'>CPU TEMPERATUR</div></div><div class='stat'><div class='value' id='cpuTempMax'>-- &deg;C</div><div class='label'>MAX. TEMPERATUR</div></div>");
             #else
               s.replace("@@CPU_TEMP_FIELDS@@", "");
             #endif
             #if WEB_DIAG_CRASHLOG
               s.replace("@@CRASH_CARD@@", String("<div class='card'><div class='title'>FEHLERBERICHT</div><div class='hint'>Maximal 10 echte Hardware-/Systemabstuerze. Persistenter C2-Crashlog.</div><div id='crashlog'>") + crashlog_html() + "</div><button class='btn clearlog' onclick='clearCrashlog()'>FEHLERBERICHTE L&Ouml;SCHEN</button><div id='crashStatus' class='status'></div></div>");
             #else
               s.replace("@@CRASH_CARD@@", "");
             #endif

             #if defined(CAPTIVE_PORTAL)
                 s.replace("@@RESET_SETTINGS@@", "&bull; <a href='/reset'>Reset settings</a>");
             #else
                 s.replace("@@RESET_SETTINGS@@", "");
             #endif

             #if defined(USE_DS18B20)
                 sensors.requestTemperatures(); 
                 float temp = sensors.getTempCByIndex(0);
                 s.replace("@@SENSOR@@", "DS18B20: " + String(temp) + "*C");
             #elif defined(USE_DHT)
                 float temp = dht.readTemperature();
                 float hum = dht.readHumidity();
                 s.replace("@@SENSOR@@", "DHT11/22: " + String(temp) + "*C, " + String(hum) + "rh%");
             #elif defined(USE_HSU07M)
                 float temp = read_hsu07m();
                 s.replace("@@SENSOR@@", "HSU07M: " + String(temp) + "*C");
             #elif defined(USE_INTERNAL_SENSOR)
                 float temp = 0;
                 temp_sensor_read_celsius(&temp);
                 s.replace("@@SENSOR@@", "CPU: " + String(temp) + "*C");
             #else
                 s.replace("@@SENSOR@@", "None");
             #endif
                 
             server.send(200, "text/html", s);
        }

        #if defined(DISPLAY_NMTV154)
        void dashboard_wallet_refresh() {
            nm_wallet_last_attempt = millis();
            const bool ok = update_wallet_data();
            if (ok) {
              server.send(200, "text/plain", "OK");
            } else {
              server.send(503, "text/plain", "Wallet-Aktualisierung fehlgeschlagen");
            }
        }

        void dashboard_wallet_interval_save() {
            if (!server.hasArg("value")) {
                server.send(400, "text/plain", "value fehlt");
                return;
            }
            long value = server.arg("value").toInt();
            if (value < 60 || value > 86400) {
                server.send(400, "text/plain", "Erlaubt: 60 bis 86400 Sekunden");
                return;
            }
            nm_save_wallet_interval((uint32_t)value);
            server.send(200, "text/plain", "Gespeichert: " + String(nm_wallet_interval_seconds) + " Sekunden");
        }

        void dashboard_brightness_live() {
            if (!server.hasArg("value")) {
                server.send(400, "text/plain", "value fehlt");
                return;
            }
            int value = constrain(server.arg("value").toInt(), 0, 100);
            nm_apply_brightness((uint8_t)value);
            server.send(200, "text/plain", String(value));
        }

        void dashboard_brightness_save() {
            if (!server.hasArg("value")) {
                server.send(400, "text/plain", "value fehlt");
                return;
            }
            int value = constrain(server.arg("value").toInt(), 0, 100);
            nm_save_brightness((uint8_t)value);
            server.send(200, "text/plain", "Gespeichert: " + String(value) + "%");
        }
        #endif

        void dashboard_crashlog_clear() {
            crashlog_clear();
            server.send(200, "text/plain", "Fehlerberichte geloescht");
        }

        void dashboard_api_status() {
            JsonDocument doc;
            doc["uptime_sec"] = (uint64_t)(millis()/1000UL);
            doc["hashrate"] = (hashrate+hashrate_core_two)/1000.0f;
            doc["difficulty"] = difficulty/100;
            doc["accepted"] = accepted_share_count;
            doc["rejected"] = rejected_share_count;
            doc["node"] = String(node_id);
            doc["free_heap"] = ESP.getFreeHeap();
            #if defined(DISPLAY_NMTV154)
              doc["wallet_balance"] = duco_wallet_valid ? duco_balance_raw : String("--");
              doc["wallet_usd"] = duco_wallet_valid ? nm_wallet_usd_text() : String("--");
              doc["wallet_last_update"] = wallet_last_update_text();
            #endif
            #if WEB_DIAG_CPU_TEMP
              doc["cpu_temp"] = nm_cpu_temp;
              doc["cpu_temp_max"] = nm_cpu_temp_max;
            #endif
            String out; serializeJson(doc,out);
            server.sendHeader("Cache-Control","no-store");
            server.send(200,"application/json",out);
        }

        void dashboard_api_diag() {
            JsonDocument doc;
            #if WEB_DIAG_CRASHLOG
              doc["crashlog_html"] = crashlog_html();
            #endif
            String out; serializeJson(doc,out);
            server.sendHeader("Cache-Control","no-store");
            server.send(200,"application/json",out);
        }

        void dashboard_diag_tempinterval() {
            if(!server.hasArg("value")){server.send(400,"text/plain","value fehlt");return;}
            uint32_t v=(uint32_t)server.arg("value").toInt();
            if(v<1||v>3600){server.send(400,"text/plain","Erlaubt: 1 bis 3600 Sekunden");return;}
            nm_temp_interval_seconds=v;
            Preferences p; p.begin("nm_diag",false); p.putUInt("tempint",v); p.end();
            nm_temp_update(true);
            server.send(200,"text/plain","Temperaturintervall: "+String(v)+" Sekunden");
        }

        void dashboard_restart() {
            server.send(200, "text/plain", "Neustart...");
            delay(150);
            RestartESP("Web dashboard restart");
        }
    #endif

} // End of namespace

MiningJob *job[CORE];

#if CORE == 2
  EasyFreeRTOS32 task1, task2;
#endif

void task1_func(void *) {
    #if defined(ESP32) && CORE == 2
      VOID SETUP() { }

      VOID LOOP() {
        job[0]->mine();

        // Zusaetzliche kurze Freigabe nach einem kompletten Job.
        // Der eigentliche WDT/Yield-Timer sitzt jetzt pro Core in MiningJob.h.
        vTaskDelay(1);

        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
           unsigned long now = millis();
           if (now - lastDisplayUpdate >= 3000) {
             lastDisplayUpdate = now;
             float hashrate_float = (hashrate+hashrate_core_two) / 1000.0;
             float accept_rate = (share_count > 0) ? (accepted_share_count / 0.01 / share_count) : 0;
             long millisecs = now;
             int uptime_secs = int((millisecs / 1000) % 60);
             int uptime_mins = int((millisecs / (1000 * 60)) % 60);
             int uptime_hours = int((millisecs / (1000 * 60 * 60)) % 24);
             String uptime = String(uptime_hours) + "h" + String(uptime_mins) + "m" + String(uptime_secs) + "s";
             float sharerate = share_count / (millisecs / 1000.0);
             display_mining_results(String(hashrate_float, 1), String(rejected_share_count), String(accepted_share_count), String(uptime), 
                                    String(node_id), String(difficulty / 100), String(sharerate, 1),
                                    String(job_count % 1000000UL), String(accept_rate, 1));
           }
        #endif
      }
    #endif
}

void task2_func(void *) {
    #if defined(ESP32) && CORE == 2
      VOID SETUP() {
        job[1] = new MiningJob(1, configuration);
      }

      VOID LOOP() {
        job[1]->mine();
        // Display wird nur in task1_func aktualisiert (kein doppeltes Rendern)
      }
    #endif
}

void setup() {
    crashlog_begin();
    #if defined(DISPLAY_NMTV154)
      nm_diag_load();
      nm_temp_update(true);
    #endif
    crashlog_boot_checkpoint(120);

    #if !defined(ESP8266) && defined(DISABLE_BROWNOUT)
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    #endif
    
    #if defined(SERIAL_PRINTING)
        Serial.begin(SERIAL_BAUDRATE);
    #endif
    #if defined(SERIAL_PRINTING)
        Serial.println("\n\nDuino-Coin " + String(configuration->MINER_VER));
    #endif
    pinMode(LED_BUILTIN, OUTPUT);

    #if defined(BLUSHYBOX)
        analogWrite(LED_BUILTIN, 255);
        for (int i = 255; i > 0; i--) {
          analogWrite(LED_BUILTIN, i);
          delay(1);
        }
        pinMode(GAUGE_PIN, OUTPUT);
      
        // Gauge up and down effect on startup
        for (int i = GAUGE_MIN; i < GAUGE_MAX; i++) {
          analogWrite(GAUGE_PIN, i);
          delay(10);
        }
        for (int i = GAUGE_MAX; i > GAUGE_MIN; i--) {
          analogWrite(GAUGE_PIN, i);
          delay(10);
        }
    #endif

    #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
        crashlog_boot_checkpoint(130);
        screen_setup();
        display_boot();
        crashlog_boot_checkpoint(140);
        delay(2500);
    #endif

    assert(CORE == 1 || CORE == 2);
    WALLET_ID = String(random(0, 2811)); // Needed for miner grouping in the wallet
    crashlog_boot_checkpoint(150);
    job[0] = new MiningJob(0, configuration);
    crashlog_boot_checkpoint(160);

    #if defined(USE_DHT)
        #if defined(SERIAL_PRINTING)
          Serial.println("Initializing DHT sensor (Duino IoT)");
        #endif
        dht.begin();
        #if defined(SERIAL_PRINTING)
          Serial.println("Test reading: " + String(dht.readHumidity()) + "% humidity");
          Serial.println("Test reading: temperature " + String(dht.readTemperature()) + "Â°C");
        #endif
    #endif

    #if defined(USE_DS18B20)
        #if defined(SERIAL_PRINTING)
          Serial.println("Initializing DS18B20 sensor (Duino IoT)");
        #endif
        sensors.begin();
        sensors.requestTemperatures(); 
        #if defined(SERIAL_PRINTING)
          Serial.println("Test reading: " + String(sensors.getTempCByIndex(0)) + "Â°C");
        #endif
    #endif

    #if defined(USE_HSU07M)
        #if defined(SERIAL_PRINTING)
          Serial.println("Initializing HSU07M sensor (Duino IoT)");
          Serial.println("Test reading: " + String(read_hsu07m()) + "Â°C");
        #endif
    #endif

    #if defined(USE_INTERNAL_SENSOR)
       #if defined(SERIAL_PRINTING)
         Serial.println("Initializing internal ESP32 temperature sensor (Duino IoT)");
       #endif
       temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
       temp_sensor.dac_offset = TSENS_DAC_L2;
       temp_sensor_set_config(temp_sensor);
       temp_sensor_start();
       float result = 0;
       temp_sensor_read_celsius(&result);
       #if defined(SERIAL_PRINTING)
         Serial.println("Test reading: " + String(result) + "Â°C");
       #endif
    #endif

    WiFi.mode(WIFI_STA); // Setup ESP in client mode
    //WiFi.disconnect(true);
    #if defined(ESP8266)
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
    #else
        WiFi.setSleep(false);
    #endif
    
    #if defined(CAPTIVE_PORTAL)
        preferences.begin("duino_config", false);
        strcpy(duco_username, preferences.getString("duco_username", "username").c_str());
        strcpy(duco_password, preferences.getString("duco_password", "None").c_str());
        strcpy(duco_rigid, preferences.getString("duco_rigid", "None").c_str());
        preferences.end();
        configuration->DUCO_USER = duco_username;
        configuration->RIG_IDENTIFIER = duco_rigid;
        configuration->MINER_KEY = duco_password;
        RIG_IDENTIFIER = duco_rigid;

        String captivePortalHTML = R"(
          <title>Duino BlushyBox</title>
          <style>
            body {
              background-color: #d4bff2;
              color: #363636;
            }
            button {
              box-shadow: 0px 0px 23px -7px #051700;
              background-color:#8645ef;
              border-radius:28px;
              border:1px solid #8645ef;
              display:inline-block;
              cursor:pointer;
              color:#ffffff;
              font-family:Arial;
              font-size:18px;
              padding:8px 16px;
              text-decoration:none;
            }
            input {
              box-shadow: 0px 0px 23px -7px #051700;
              background-color:#ffffff;
              border-radius:28px;
              border:1px solid #777777;
              font-family:Arial;
              font-size:18px;
              text-decoration:none;
            }
          </style>
        )";
      
        wifiManager.setCustomHeadElement(captivePortalHTML.c_str());
        
        wifiManager.setSaveConfigCallback(saveConfigCallback);
        wifiManager.addParameter(&custom_duco_username);
        wifiManager.addParameter(&custom_duco_password);
        wifiManager.addParameter(&custom_duco_rigid);

        #if defined(BLUSHYBOX)
          blinker.attach_ms(200, changeState);
        #endif
        wifiManager.autoConnect("Duino-Coin");
        delay(1000);
        VerifyWifi();
        #if defined(BLUSHYBOX)
          blinker.detach();
        #endif
        
        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
            display_info("Waiting for node...");
        #endif
        #if defined(BLUSHYBOX)
          blinker.attach_ms(500, changeState);
        #endif
        SelectNode();
        #if defined(BLUSHYBOX)
          blinker.detach();
        #endif
    #else
        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
          display_info("Waiting for WiFi...");
        #endif
        crashlog_boot_checkpoint(170);
        SetupWifi();
    #endif
    crashlog_boot_checkpoint(180);
    #if defined(DISPLAY_NMTV154)
      nm_backlight_setup();
      nm_load_wallet_interval();
      nm_wallet_last_attempt = millis();
      update_wallet_data();
    #endif

    crashlog_boot_checkpoint(210);
    SetupOTA();
    crashlog_boot_checkpoint(220);

    #if defined(WEB_DASHBOARD)
      crashlog_boot_checkpoint(230);
      if (!MDNS.begin(RIG_IDENTIFIER)) {
        #if defined(SERIAL_PRINTING)
          Serial.println("mDNS unavailable");
        #endif
      }
      MDNS.addService("http", "tcp", 80);
      #if defined(SERIAL_PRINTING)
        #ifdef USE_LAN
          Serial.println("Configured mDNS for dashboard on http://" + String(RIG_IDENTIFIER) 
                     + ".local (or http://" + ETH.localIP().toString() + ")");
        #else
          Serial.println("Configured mDNS for dashboard on http://" + String(RIG_IDENTIFIER) 
                     + ".local (or http://" + WiFi.localIP().toString() + ")");
        #endif
      #endif

      server.on("/", dashboard);
      #if defined(DISPLAY_NMTV154)
        server.on("/wallet/refresh", HTTP_POST, dashboard_wallet_refresh);
        server.on("/wallet/interval", HTTP_GET, dashboard_wallet_interval_save);
        server.on("/brightness", HTTP_GET, dashboard_brightness_live);
        server.on("/brightness/save", HTTP_GET, dashboard_brightness_save);
      #endif
      server.on("/crashlog/clear", HTTP_POST, dashboard_crashlog_clear);
      server.on("/api/status", HTTP_GET, dashboard_api_status);
      server.on("/api/diag", HTTP_GET, dashboard_api_diag);
      #if WEB_DIAG_CPU_TEMP
        server.on("/diag/tempinterval", HTTP_GET, dashboard_diag_tempinterval);
      #endif
      server.on("/restart", HTTP_POST, dashboard_restart);
      #if defined(CAPTIVE_PORTAL)
        server.on("/reset", reset_settings);
      #endif
      server.begin();
      crashlog_boot_checkpoint(240);
    #endif

    #if defined(ESP8266)
        // Start the WDT watchdog
        lwdtFeed();
        lwdTimer.attach_ms(LWD_TIMEOUT, lwdtcb);
    #endif

    #if defined(ESP8266)
        // Fastest clock mode for 8266s
        system_update_cpu_freq(160);
        os_update_cpu_frequency(160);
        // Feed the watchdog
        lwdtFeed();
    #else
        // C2 läuft maximal mit 120 MHz; klassische ESP32 mit 240 MHz.
        #if defined(CONFIG_IDF_TARGET_ESP32C2)
          setCpuFrequencyMhz(120);
        #else
          setCpuFrequencyMhz(240);
        #endif
    #endif

    job[0]->blink(BLINK_SETUP_COMPLETE);

    #if defined(ESP32) && CORE == 2
      mutexClientData = xSemaphoreCreateMutex();
      mutexConnectToServer = xSemaphoreCreateMutex();

      xTaskCreatePinnedToCore(system_events_func, "system_events_func", 10000, NULL, 1, NULL, 0);
      xTaskCreatePinnedToCore(task1_func, "task1_func", 10000, NULL, 1, &Task1, 0);
      xTaskCreatePinnedToCore(task2_func, "task2_func", 10000, NULL, 1, &Task2, 1);
    #endif
    crashlog_boot_checkpoint(270);
}

void system_events_func(void* parameter) {
  while (true) {
    delay(10);
    #if defined(WEB_DASHBOARD)
      server.handleClient();
    #endif
    ArduinoOTA.handle();

    #if defined(DISPLAY_NMTV154)
      // C2-Pult hat keinen Touch.

      // Uhr unabhängig vom Miner-Refresh aktualisieren.
      // Gezeichnet wird nur beim Minutenwechsel; die Prüfung erfolgt häufig,
      // damit z.B. 20:57 -> 20:58 direkt bei Sekunde 00 umspringt.
      update_display_clock();
    #endif

    // Wallet-Timer läuft zentral in duco_background_events().
  }
}

void single_core_loop() {
    job[0]->mine();
    crashlog_heartbeat(share_count, accepted_share_count, millis());
    
    lwdtFeed();
    
    #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_NMTV154)
       // Display nur alle 3000ms aktualisieren - spart ~50% Rechenzeit beim SSD1306!
       #define DISPLAY_UPDATE_INTERVAL 3000
       unsigned long now = millis();
       if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
         lastDisplayUpdate = now;

         float hashrate_float = (hashrate+hashrate_core_two) / 1000.0;
         float accept_rate = (share_count > 0) ? (accepted_share_count / 0.01 / share_count) : 0;
         
         long millisecs = now;
         int uptime_secs = int((millisecs / 1000) % 60);
         int uptime_mins = int((millisecs / (1000 * 60)) % 60);
         int uptime_hours = int((millisecs / (1000 * 60 * 60)) % 24);
         String uptime = String(uptime_hours) + "h" + String(uptime_mins) + "m" + String(uptime_secs) + "s";
         
         float sharerate = share_count / (millisecs / 1000.0);

         display_mining_results(String(hashrate_float, 1), String(rejected_share_count), String(accepted_share_count), String(uptime), 
                                String(node_id), String(difficulty / 100), String(sharerate, 1),
                                String(job_count % 1000000UL), String(accept_rate, 1));
       }
    #endif

    // System-Events nur alle 5 Sekunden prüfen (nicht bei jedem Mine-Aufruf!)
    static unsigned long lastSysCheck = 0;
    unsigned long nowSys = millis();
    if (nowSys - lastSysCheck >= 5000) {
      lastSysCheck = nowSys;
      VerifyWifi();
    }
    #if defined(DISPLAY_NMTV154)
      nm_temp_update();
    #endif
    ArduinoOTA.handle();
    #if defined(WEB_DASHBOARD) 
        server.handleClient();
    #endif
}

void loop() {
  #if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_FREERTOS_UNICORE)
    single_core_loop();
  #elif defined(ESP32) && CORE == 2
    // Dual-Core: Mining läuft in Tasks, loop() nur für System-Events
    delay(10);
  #endif
}
