
/* ==========================================================================================
   FM Receiver with MQTT
   Sketch to control FM Receiver with SI4703 Chip
   MQTT is optional, but without it control and changes to some options will need to be implmented
   in another manner, such as hardware rotory encoders, push buttons, touch screen, etc.

   Standard pins for ESP8266: SDA:D2, SCL:D1, RESET:D5
   Standard pins for ESP32: SDA:21, SCL:22, RESET:17
   
   Boards Tested [ Arduino Board selected ]: 
     Wemos D1 Mini ESP8266 [ LOLIN(WEMOS) D1, R2 & mini ]
     ESP32 D1 Mini [ ESP32 Dev Module ]
     
   v1.00 Nov. 2023 - Initial public release
   Resinchem Tech - licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
   =========================================================================================*/

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WiFi.h>               
#include <ESPmDNS.h>              // https://github.com/espressif/arduino-esp32
#include <WebServer.h>  
#endif
#include <WiFiUdp.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Wire.h>
#include "Credentials.h"          //File must exist in same folder as .ino.  Edit as needed for project
#include "Settings.h"             //File must exist in same folder as .ino.  Edit as needed for project

// Libraries for FM Receiver ST4703
#include <radio.h>               // More documentation is available at http://www.mathertel.de/Arduino
#include <SI4703.h>              // Source Code is available on https://github.com/mathertel/Radio
#include <SI47xx.h>
#include <RDSParser.h>
// -------------------

// ==========================================
//   UPDATE THE NEXT TWO SECTIONS FOR YOUR
//   PARTICULAR NETWORK AND LOCATION
// ==========================================

/* -------------------------------------------------------
 *  Client and Host Names - Must be unique on your network
 * ------------------------------------------------------- */
//Define different MQTT Client, Wifi Host and OTA Host names for ESP8266 and ESP32 so they can be swapped/co-exist on same network
#if defined(ESP8266)
  #define MQTTCLIENT "fm_receiver"             // MQTT Client Name for ESP8266
  #define WIFIHOSTNAME "FM_RECEIVER"           // Host name for WiFi/Router ESP8266
  #define OTA_HOSTNAME "FM_RECEIVER_OTA"       // Hostname to broadcast as port in the IDE of OTA Updates
#elif defined(ESP32)
  #define MQTTCLIENT "fm_receiver_32"          // MQTT Client Name
  #define WIFIHOSTNAME "FM_RECEIVER_32"        // Host name for WiFi/Router
  #define OTA_HOSTNAME "FM_RECEIVER_OTA_32"    // Hostname to broadcast as port in the IDE of OTA Updates
#endif

/* -------------------------------------
 *  FM Receiver Presets Array - Edit/Add 
 * ------------------------------------- */
// Define some station presets available at your locations here:
// 89.30 MHz as 8930
RADIO_FREQ preset[] = {
  9470,   // Sender: WFBQ Q95
  10450,  // Sender: WJJK Classic Hits
  9710,   // Sender: WLHK Hank FM
  9550,   // Sender: WFMS 95.5
  9950,   // Sender: WZPL 
  9310,   // Sender: WIBC Talk
  9830,   // Sender: WZRL
  10670   // Sender: WTLC
};

// =======================================
// **** END UPDATE SECTION ****
// =======================================

#if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
  #include <PubSubClient.h>
#endif

//GLOBAL VARIABLES (MQTT and OTA Updates)
bool mqttConnected = false;               //Will be enabled if defined and successful connnection made.  This var should be checked upon any MQTT actin.
long lastReconnectAttempt = 0;            //If MQTT connected lost, attempt reconnenct
uint16_t ota_time = ota_boot_time_window;
uint16_t ota_time_elapsed = 0;            // Counter when OTA active

//Other Globals
bool firstBoot = true;                    //Needed for logging boot time, which is not available during setup.
unsigned long prevTime = 0;               //Main loop timer
bool simulatedPowerOn = true;             //Needed to simulate power on/off
char lastRDSName[66];
char lastRDSText[66];

//Setup Local Access point if enabled via WIFI Mode
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2)
  const char* APssid = AP_SSID;        
  const char* APpassword = AP_PWD;  
#endif

//Setup Wifi if enabled via WIFI Mode
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2)
  const char *ssid = SID;
  const char *password = PW;
#endif

//Setup MQTT if enabled - only available when WiFi is also enabled
#if (WIFIMODE == 1 || WIFIMODE == 2) && (MQTTMODE == 1)    // MQTT only available when on local wifi
  const char *mqttUser = MQTTUSERNAME;
  const char *mqttPW = MQTTPWD;
  const char *mqttClient = MQTTCLIENT;
  const char *mqttTopicSub = MQTT_TOPIC_SUB;
 // const char *mqttTopicPub = MQTT_TOPIC_PUB;
#endif

WiFiClient espClient;
#if defined(ESP8266)
ESP8266WebServer server;
#elif defined(ESP32)
WebServer server;
#endif
#if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
  PubSubClient client(espClient);
#endif

uint16_t presetIndex = bootPreset;  ///< Start at preset indicated in Settings.h

// ===== SI4703 specific pin wiring =====
#define ENABLE_SI4703

#ifdef ENABLE_SI4703

#if defined(ESP8266)
#define RESET_PIN D5
#define MODE_PIN D2  // same as SDA

#elif defined(ESP32)
#define RESET_PIN 17
#define MODE_PIN 21  // same as SDA

#endif
#endif

// Standard I2C/Wire pins for ESP8266: SDA:D2, SCL:D1
// Standard I2C/Wire pins for ESP32: SDA:21, SCL:22

/// The radio object has to be defined by using the class corresponding to the used chip.
// RADIO radio;     ///< Create an instance of a non functional radio.
SI4703 radio;  ///< Create an instance of a SI4703 chip radio.

/// get a RDS parser
RDSParser rds;


/// State of Keyboard input for this radio implementation.
enum RADIO_STATE {
  STATE_PARSECOMMAND,  ///< waiting for a new command character.
  STATE_PARSEINT,      ///< waiting for digits for the parameter.
  STATE_EXEC           ///< executing the command.
};

RADIO_STATE kbState;  ///< The state of parsing input characters.
char kbCommand;
int16_t kbValue;

bool lowLevelDebug = false;
// ==========================
//  End FM Receiver Setup
// ==========================

//------------------------------
// Setup WIFI
//-------------------------------
void setup_wifi() {
#if defined(ESP8266)  
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  //Disable WiFi Sleep
#elif defined(ESP32)
  WiFi.setSleep(false);
#endif
  delay(200);
  // WiFi - AP Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2) 
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(APssid, APpassword);    // IP is usually 192.168.4.1
#endif
  // WiFi - Local network Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2) 
  byte count = 0;

  WiFi.hostname(WIFIHOSTNAME);
  WiFi.begin(ssid, password);
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.print("Connecting to WiFi");
  #endif
  while (WiFi.status() != WL_CONNECTED) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.print(".");
    #endif
    // Stop if cannot connect
    if (count >= 60) {
      // Could not connect to local WiFi 
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println();
        Serial.println("Could not connect to WiFi.");   
      #endif  
      return;
    }
    delay(500);
    count++;
  }
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println();
    Serial.println("Successfully connected to Wifi");
    IPAddress ip = WiFi.localIP();
    Serial.println(WiFi.localIP());
  #endif
#endif   
};

// ===================================
//  SETUP MQTT AND CALLBACKS
// ===================================
void setup_mqtt() {
#if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
  byte mcount = 0;
  //char topicPub[32];
  client.setServer(MQTTSERVER, MQTTPORT);
  client.setCallback(callback);
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.print("Connecting to MQTT broker.");
  #endif
  while (!client.connected( )) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.print(".");
    #endif
    client.connect(mqttClient, mqttUser, mqttPW);
    if (mcount >= 60) {
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println();
        Serial.println("Could not connect to MQTT broker. MQTT disabled.");
      #endif
      // Could not connect to MQTT broker
      return;
    }
    delay(500);
    mcount++;
  }
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println();
    Serial.println("Successfully connected to MQTT broker.");
  #endif
  client.subscribe(MQTT_TOPIC_SUB"/#");
  client.publish(MQTT_TOPIC_PUB"/mqtt", "connected", true);
  //Publish IP address
  updateMqttIPAddress();
  
  mqttConnected = true;
#endif
}

void reconnect() {
  int retries = 0;
#if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
  while (!client.connected()) {
    if(retries < 150)
    {
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.print("Attempting MQTT connection...");
      #endif
      if (client.connect(mqttClient, mqttUser, mqttPW)) 
      {
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          Serial.println("connected");
        #endif
        // ... and resubscribe
        client.subscribe(MQTT_TOPIC_SUB"/#");
      } 
      else 
      {
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
          Serial.print("failed, rc=");
          Serial.print(client.state());
          Serial.println(" try again in 5 seconds");
        #endif
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if(retries > 149)
    {
    ESP.restart();
    }
  }
#endif
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  /*
   * Add any commands submitted here
   * Example:
   * if (strcmp(topic, "cmnd/matrix/mode")==0) {
   *   MyVal = message;
   *   Do something
   *   return;
   * };
   */
  if (strcmp(topic, MQTT_TOPIC_SUB"/volume") == 0) {
  // Set Volume
    byte vol = message.toInt();
    if ((vol >= 0) && (vol <= radio.getMaxVolume())) {
      radio.setVolume(vol);
    }
    updateMqttVolume();
    
  } else if (strcmp(topic, MQTT_TOPIC_SUB"/preset_up") == 0 ) {
  // Next Preset - wrap to zero if at max
    if (presetIndex < (sizeof(preset) / sizeof(RADIO_FREQ)) - 1) {
      presetIndex++;
    } else {
      presetIndex = 0;  
    }
    radio.setFrequency(preset[presetIndex]);
    radio.clearRDS();
    updateMqttPreset();

  } else if (strcmp(topic, MQTT_TOPIC_SUB"/preset_down") == 0 ) {
  // Previous Preset - wrap to last if at first
    if (presetIndex > 0) {
      presetIndex--;
    } else {
      presetIndex = (sizeof(preset) / sizeof(RADIO_FREQ));      
    }
    radio.setFrequency(preset[presetIndex]);
    radio.clearRDS();
    updateMqttPreset();
    
  } else if (strcmp(topic, MQTT_TOPIC_SUB"/preset_num") == 0 ) {
  // Jump to specific preset (passed as 1 based, but array is zero based)  
    int passedPreset = message.toInt();
    if ((passedPreset > 0) && (passedPreset <= (sizeof(preset) / sizeof(RADIO_FREQ)))) {
      presetIndex = passedPreset - 1;
      radio.setFrequency(preset[presetIndex]);
      radio.clearRDS();
      updateMqttPreset();
    }

  } else if (strcmp(topic, MQTT_TOPIC_SUB"/frequency") == 0 ) {
  // Tune directly to specified frequency
    int16_t passedFreq = message.toInt();
    radio.setFrequency(passedFreq);
    radio.clearRDS();
    updateMqttFrequency();      

  } else if (strcmp(topic, MQTT_TOPIC_SUB"/mute") == 0) {
  // Mute (defaults to 'off' if message does not match on options)
    if ((message == "ON") || (message == "on") || (message == "1")) {
      radio.setMute(true);    
    } else {
      radio.setMute(false);
    }
    updateMqttMute();

  } else if (strcmp(topic, MQTT_TOPIC_SUB"/softmute") == 0) {
  // Mute (defaults to 'off' if message does not match on options)
    if ((message == "ON") || (message == "on") || (message == "1")) {
      radio.setSoftMute(true);    
    } else {
      radio.setSoftMute(false);
    }
    updateMqttSoftMute();
    

  } else if (strcmp(topic, MQTT_TOPIC_SUB"/stereo") == 0) {
    if ((message == "ON") || (message == "on") || (message == "1")) {
      radio.setMono(false);    
    } else {
      radio.setMono(true);
    }
    updateMqttStereo();
    
  } else if (strcmp(topic, MQTT_TOPIC_SUB"/power") == 0) {
    if ((message == "ON") || (message == "on") || (message == "1")) {
      powerOn(true);
    } else {
      powerOn(false);
    }
    
  }
  
}

// ==========================
//  FM Receiver Functions
// ==========================
/// Update the Frequency on the LCD display here.
void DisplayFrequency() {
  char s[12];
  radio.formatFrequency(s, sizeof(s));
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.print("FREQ:");
    Serial.println(s);
  #endif
}  // DisplayFrequency()


/// Update the ServiceName text on the LCD display here.
void DisplayServiceName(const char *name) {

  //Only update MQTT if simulated power is on and RDS name has changed from previous update
  if ((simulatedPowerOn) && (strcmp(name, lastRDSName) != 0  )) {
    updateMqttRDSName(name);
    strcpy(lastRDSName, name);
  }
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.print("RDS Service Name: ");
    Serial.println(name);
  #endif

}  

/// Update RDS text on the LCD display here.
void DisplayTextInfo(const char *name) {
  if ((simulatedPowerOn) && (strcmp(name, lastRDSText) != 0  )) {
    updateMqttRDSText(name);
    strcpy(lastRDSText, name);
  }
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.print("RDS Text: ");
    Serial.println(name);
  #endif
}

void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {
  rds.processData(block1, block2, block3, block4);
}


/// Execute a command identified by a character and an optional number.
/// See the "?" command for available commands.
/// \param cmd The command character.
/// \param value An optional parameter for the command.
/// Only enabled if SERIAL_DEBUG is enabled in Settings.h
void runSerialCommand(char cmd, int16_t value) {
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    if (cmd == '?') {
      Serial.println();
      Serial.println("? Help");
      Serial.println("+ increase volume");
      Serial.println("- decrease volume");
      Serial.println("> next preset");
      Serial.println("< previous preset");
      Serial.println(". scan up   : scan up to next sender");
      Serial.println(", scan down ; scan down to next sender");
      Serial.println("fnnnnn: direct frequency input");
      Serial.println("i station status");
      Serial.println("s mono/stereo mode");
      Serial.println("b bass boost");
      Serial.println("u mute/unmute");
    }
  
    // ----- control the volume and audio output -----
  
    else if (cmd == '+') {
      // increase volume
      int v = radio.getVolume();
      radio.setVolume(++v);
    } else if (cmd == '-') {
      // decrease volume
      int v = radio.getVolume();
      if (v > 0)
        radio.setVolume(--v);
    }
  
    else if (cmd == 'u') {
      // toggle mute mode
      radio.setMute(!radio.getMute());
    }
  
    // toggle stereo mode
    else if (cmd == 's') {
      radio.setMono(!radio.getMono());
    }
  
    // toggle bass boost
    else if (cmd == 'b') {
      radio.setBassBoost(!radio.getBassBoost());
    }
  
    // ----- control the frequency -----
  
    else if (cmd == '>') {
      // next preset
      if (presetIndex < (sizeof(preset) / sizeof(RADIO_FREQ)) - 1) {
        presetIndex++;
        radio.setFrequency(preset[presetIndex]);
        radio.clearRDS();
      }  // if
    } else if (cmd == '<') {
      // previous preset
      if (presetIndex > 0) {
        presetIndex--;
        radio.setFrequency(preset[presetIndex]);
        radio.clearRDS();
      }  // if
  
    } else if (cmd == 'f') {
      radio.setFrequency(value);
      radio.clearRDS();
    }
  
    else if (cmd == '.') {
      radio.seekUp(false);
    } else if (cmd == ':') {
      radio.seekUp(true);
    } else if (cmd == ',') {
      radio.seekDown(false);
    } else if (cmd == ';') {
      radio.seekDown(true);
    }
  
  
    else if (cmd == '!') {
      // not in help
      RADIO_FREQ f = radio.getFrequency();
      if (value == 0) {
        radio.term();
      } else if (value == 1) {
        radio.initWire(Wire);
        radio.setBandFrequency(RADIO_BAND_FM, f);
        radio.setVolume(10);
      }
  
    } else if (cmd == 'i') {
      // info
      char s[12];
      radio.formatFrequency(s, sizeof(s));
      Serial.print("Station:");
      Serial.println(s);
      Serial.print("Radio:");
      radio.debugRadioInfo();
      Serial.print("Audio:");
      radio.debugAudioInfo();
  
    } else if (cmd == 'x') {
      radio.debugStatus();  // print chip specific data.
  
    } else if (cmd == '*') {
      lowLevelDebug = !lowLevelDebug;
      radio._wireDebug(lowLevelDebug);
    }
  #endif
}  // runSerialCommand()

void powerOn(bool turnOn) {
  //The receiver/library does not have an on/off state
  //To simulate, the output will be muted when turned 'off'
  //and all MQTT values set to reflect 'off' state
  if (!turnOn) {
    //simulate power off
    radio.setMute(true);
    updateMqttPowerDown();
    simulatedPowerOn = false;
    radio.clearRDS();
  } else {
    radio.setMute(false);
    updateMqttAll();
    updateMQTTPowerOn();
    simulatedPowerOn = true;
    radio.clearRDS();
  }
}

// =============================================
//    MAIN SETUP
// =============================================
/// Setup a FM only radio configuration with I/O for commands and debugging on the Serial port.
void setup() {
  byte i;
  byte j;
  delay(2000);
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.begin(115200);
    Serial.println("Starting setup....");
  #endif
  delay(200);

 //-----------------------------
  // Setup Wifi
  //-----------------------------
  setup_wifi();

  //-----------------------------
  // Setup OTA Updates
  //-----------------------------
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
  });
  ArduinoOTA.begin();
  // Setup handlers for web calls for OTAUpdate and Restart
  server.on("/restart",[](){
    server.send(200, "text/html", "<h1>Restarting...</h1>");
    delay(1000);
    ESP.restart();
  });
  server.on("/otaupdate",[]() {
    server.send(200, "text/html", "<h1>FM Receiver Ready for Update...<h1><h3>Start upload from IDE now</h3>");
    ota_flag = true;
    ota_time = ota_time_window;
    ota_time_elapsed = 0;
  });
  server.begin();

  //------------------------
  // Setup MQTT if Enabled
  //------------------------
  setup_mqtt();

  // ----------------------------
  //   FM Receiver Setup
  // ----------------------------
#if defined(RESET_PIN)
  // This is required for SI4703 chips:
  radio.setup(RADIO_RESETPIN, RESET_PIN);
  radio.setup(RADIO_MODEPIN, MODE_PIN);
#endif

  // Enable information to the Serial port
  radio.debugEnable(true);
  radio._wireDebug(lowLevelDebug);

  // Set FM Options for Europe
  //radio.setup(RADIO_FMSPACING, RADIO_FMSPACING_100);   // for EUROPE
  //radio.setup(RADIO_DEEMPHASIS, RADIO_DEEMPHASIS_50);  // for EUROPE
  radio.setup(RADIO_FMSPACING, RADIO_FMSPACING_200);   // for U.S.
  radio.setup(RADIO_DEEMPHASIS, RADIO_DEEMPHASIS_75);  // for U.S.


  // Initialize the Radio
  if (!radio.initWire(Wire)) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("no radio chip found.");
    #endif
    delay(4000);
    while (1) {};
  };

  radio.setBandFrequency(RADIO_BAND_FM, preset[presetIndex]);

  //radio.setMono(false);
  //radio.setMute(false);
  //radio.setVolume(radio.getMaxVolume() / 2);

  radio.setMono(!bootStereo);
  radio.setMute(bootMute);
  radio.setSoftMute(bootSoftMute);
  radio.setVolume(bootVolume);

  // setup the information chain for RDS data.
  radio.attachReceiveRDS(RDS_process);
  rds.attachServiceNameCallback(DisplayServiceName);
  rds.attachTextCallback(DisplayTextInfo);

  runSerialCommand('?', 0);
  kbState = STATE_PARSECOMMAND;

#if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
  updateMqttAll();
  updateMQTTPowerOn();
#endif
}  // Setup


// =============================================
//    MAIN LOOP
// =============================================
/// Constantly check for serial input commands and trigger command execution.
void loop() {
  unsigned long now = millis();
  static unsigned long nextFreqTime = 0;
  unsigned long nextRDSTime = 0;

  //Handle OTA updates when OTA flag set via HTML call to http://ip_address/otaupdate
  if (ota_flag) {
    uint16_t ota_time_start = millis();
    while (ota_time_elapsed < ota_time) {
      ArduinoOTA.handle();  
      ota_time_elapsed = millis()-ota_time_start;   
      delay(10); 
    }
    ota_flag = false;
  }

  //Handle any web calls
  server.handleClient();
  //MQTT Calls
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    if (!client.connected()) 
    {
      reconnect();
    }
    client.loop();
  #endif


  // some internal static values for parsing the input
  static RADIO_FREQ lastFrequency = 0;
  RADIO_FREQ f = 0;
  //Only enable if SERIAL DEBUG enabled in Settings.h
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    if (Serial.available() > 0) {
      // read the next char from input.
      char c = Serial.peek();
  
      if ((kbState == STATE_PARSECOMMAND) && (c < 0x20)) {
        // ignore unprintable chars
        Serial.read();
  
      } else if (kbState == STATE_PARSECOMMAND) {
        // read a command.
        kbCommand = Serial.read();
        kbState = STATE_PARSEINT;
  
      } else if (kbState == STATE_PARSEINT) {
        if ((c >= '0') && (c <= '9')) {
          // build up the value.
          c = Serial.read();
          kbValue = (kbValue * 10) + (c - '0');
        } else {
          // not a value -> execute
          runSerialCommand(kbCommand, kbValue);
          kbCommand = ' ';
          kbState = STATE_PARSECOMMAND;
          kbValue = 0;
        }  // if
      }    // if
    }      // if
  #endif
  // check for RDS data
  if ((now > nextRDSTime) && (simulatedPowerOn)) {
    radio.checkRDS();
    nextRDSTime = now + rds_check_time;
  }
  // update the display from time to time
  if (now > nextFreqTime) {
    f = radio.getFrequency();
    if (f != lastFrequency) {
      // print current tuned frequency
      DisplayFrequency();
      lastFrequency = f;
    }  // if
    nextFreqTime = now + 400;
  }  // if

} 

// ==========================
//  MQTT Messaging
// ==========================
void updateMqttAll() {
  updateMqttVolume();
  updateMqttFrequency();  //will also update preset
  updateMqttMute();
  updateMqttSoftMute();
  updateMqttStereo();
  //Power and IP address handled separately
}

void updateMqttVolume() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    int v = radio.getVolume();
    char outMsg[4];
    sprintf(outMsg, "%2u", v);
    client.publish(MQTT_TOPIC_PUB"/volume", outMsg, true);
  #endif
}

void updateMqttPreset() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    //Add one to the presetIndex, since it is zero-based
    byte p = presetIndex + 1;
    char outMsg[4];
    sprintf(outMsg, "%3u", p);
    client.publish(MQTT_TOPIC_PUB"/preset", outMsg, true);
    //Also needs to update frequency
    updateMqttFrequency();

  #endif
}

void updateMqttFrequency() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    char s[12];
    radio.formatFrequency(s, sizeof(s));
    int16_t rawFreg = radio.getFrequency();
    //Publish both formatted and raw frequency
    client.publish(MQTT_TOPIC_PUB"/frequency", s, true);
    char outMsg[8];
    sprintf(outMsg, "%6u", rawFreg);
    client.publish(MQTT_TOPIC_PUB"/rawfreq", outMsg, true);
    //Check to see if the frequency matches a preset
    int aSize = (sizeof(preset) / sizeof(RADIO_FREQ));
    int presetMatch = -1;
    RADIO_FREQ f = radio.getFrequency();
    for (int x = 0; x < aSize; x++) {
       if (preset[x] == f) {
          presetMatch = x;
       }
    }
    if (presetMatch >= 0) {
      //match found, set presetIndex and publish MQTT
      presetIndex = presetMatch;
      char outMsg[4];
      byte p = presetIndex + 1;
      sprintf(outMsg, "%3u", p);
      client.publish(MQTT_TOPIC_PUB"/preset", outMsg, true);
    } else {
      //Current frequency does not match any presets - publish 0 but do not change internal index.
      client.publish(MQTT_TOPIC_PUB"/preset", "0", true);
    }

  #endif

}

void updateMqttMute() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    if ( radio.getMute() ) {
      client.publish(MQTT_TOPIC_PUB"/mute", "on", true);
    } else {
      client.publish(MQTT_TOPIC_PUB"/mute", "off", true);
    }
  #endif
}

void updateMqttSoftMute() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    if ( radio.getSoftMute() ) {
      client.publish(MQTT_TOPIC_PUB"/softmute", "on", true);
    } else {
      client.publish(MQTT_TOPIC_PUB"/softmute", "off", true);
    }
  #endif
}

void updateMqttStereo() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    if ( radio.getMono() ) {
      client.publish(MQTT_TOPIC_PUB"/stereo", "off", true);
    } else {
      client.publish(MQTT_TOPIC_PUB"/stereo", "on", true);
    }
  #endif
}


void updateMqttIPAddress() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    String ipaddr = WiFi.localIP().toString();
    byte msgLen = ipaddr.length() + 1;
    char outMsg[msgLen];
    ipaddr.toCharArray(outMsg, msgLen);
    client.publish(MQTT_TOPIC_PUB"/ipaddress", outMsg, true);
  #endif
}

void updateMqttRDSName(const char *name) {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    char outMsg[66];
    const char *spacer = " ";
    strcpy(outMsg, name);
    strcat(outMsg, spacer);
    //client.publish(MQTT_TOPIC_PUB"/rdsname", name, true);
    client.publish(MQTT_TOPIC_PUB"/rdsname", outMsg, true);
  #endif
}

void updateMqttRDSText(const char *name) {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    char outMsg[66];
    const char *spacer = " ";
    strcpy(outMsg, name);
    strcat(outMsg, spacer);
    //client.publish(MQTT_TOPIC_PUB"/rdstext", name, true);
    client.publish(MQTT_TOPIC_PUB"/rdstext", outMsg, true);
  #endif
}


void updateMqttPowerDown() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    //simulated power off values
    //client.publish(MQTT_TOPIC_PUB"/volume", "0", true);
    client.publish(MQTT_TOPIC_PUB"/frequency", " ", true);
    client.publish(MQTT_TOPIC_PUB"/preset", "0", true);
    client.publish(MQTT_TOPIC_PUB"/softmute", "off", true);
    client.publish(MQTT_TOPIC_PUB"/mute", "off", true);
    client.publish(MQTT_TOPIC_PUB"/stereo", "off", true);
    client.publish(MQTT_TOPIC_PUB"/power", "off", true);
    client.publish(MQTT_TOPIC_PUB"/rdsname", " ", true);
    client.publish(MQTT_TOPIC_PUB"/rdstext", " ", true);
    strcpy(lastRDSName, " ");
    strcpy(lastRDSText, " ");
  #endif
}

void updateMQTTPowerOn() {
  #if defined(MQTTMODE) && (MQTTMODE == 1 && (WIFIMODE == 1 || WIFIMODE == 2))
    client.publish(MQTT_TOPIC_PUB"/power", "on", true);
  #endif
  
}
