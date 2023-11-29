// =====================================================================================
// Update/Add any #define values to match your build and board type if not using D1 Mini
// =====================================================================================
// v1.00


#define WIFIMODE 2                           // 0 = Only Soft Access Point, 1 = Only connect to local WiFi network with UN/PW, 2 = Both
#define MQTTMODE 1                           // 0 = Disable MQTT, 1 = Enable (will only be enabled if WiFi mode = 1 or 2 - broker must be on same network)
#define MQTT_TOPIC_SUB "cmnd/fmrecvr"        // Default MQTT subscribe topic for commands to this device
#define MQTT_TOPIC_PUB "stat/fmrecvr"        // Default MQTT publish topic
#define SERIAL_DEBUG 1                       // 0 = Disable (must be disabled if using RX/TX pins), 1 = enable for serial monitor/input

// ---------------------------------------------------------------------------------------------------------------
// Options - Defaults upon boot-up or any other custom ssttings
// ---------------------------------------------------------------------------------------------------------------
// OTA Settings
bool ota_flag = true;                    // Must leave this as true for board to broadcast port to IDE upon boot
uint16_t ota_boot_time_window = 2500;    // minimum time on boot for IP address to show in IDE ports, in millisecs
uint16_t ota_time_window = 20000;        // time to start file upload when ota_flag set to true (after initial boot), in millsecs

// Other options
uint16_t rds_check_time = 5000;  //How often to check for RDS data
// Initial Boot values
uint16_t bootPreset = 0;       //Initial preset station to load at boot
uint8_t bootVolume = 8;        //Intitial boot volume (0 - 15)
bool bootMute = false;         //Boot with mute enabled
bool bootSoftMute = true;      //Boot with soft mute enabled
bool bootStereo = true;        //Boot in stereo mode
