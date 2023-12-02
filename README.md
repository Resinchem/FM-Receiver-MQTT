## FM Receiver with MQTT
This repo simply adds wrapper functionality to the excellent [RADIO library by mathertel](https://github.com/mathertel/Radio).  You can find much more information on use of the radio library in his repo.  It really won't be covered here.

The wrapper adds the following functionality:

- WIFI (obviously required for OTA updates and MQTT)
- OTA Updates
- MQTT publish and subscribe topics
- A few other minor options, such as the ability to disable Serial communication when using WiFi

### Supported Hardware

This particular version is written specifically for the SI4703 FM Receiver chip.

![SI4703](https://github.com/Resinchem/FM-Receiver-MQTT/assets/55962781/57ae9b03-3bef-4af6-a9a1-4f8f9c92abc2)

But the RADIO library supports a number of other FM boards.  Please see the [RADIO repo](https://github.com/mathertel/Radio) for more information on implementing for other chipsets.  This version utilizes version 3.0.1 of the library, which was the most current at time of publication.  It likely will not be update if future version of the radio library are released.

This wrapper version was primarily developed and tested using a Wemos D1 Mini (ESP8266), but was also confirmed to run on an ESP32 via preprocessor directives.  Basic functionality was confirmed to work, but testing was not as thorough as it was on the D1 Mini.

### MQTT

The primary purpose of this wrapper is to provide MQTT for integrating the FM Receiver into Home Assistant (or other compatible MQTT automation system)

<< PHOTO_02_HA_DASHBOARD>>


You can see more info on wiring and implementation in a [YouTube video](https://youtu.be/BJh-vUknn-Q) 

Sample Home Assistant YAML code for implementing MQTT entities and creating the dashboard shown above are included in the /homeassistant folder.

#### Please see the [WIKI](https://github.com/Resinchem/FM-Receiver-MQTT/wiki) for some general setup and install information, along with a list of available MQTT topics.
