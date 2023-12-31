### Home Assistant Example Files

These are some **sample** files you can use as guides for your own Home Assistant installation to integrate and control the FM Receiver in Home Assistant.

It is unlikely that you will be able to just copy/paste these into your Home Assistant.  Each entity or integration type should be placed in the appropriate location within your own Home Assistant (e.g. sensors, switches, automations, etc.)

Note:  These example use the MQTT topics as defined in the fm-receiver source code.  If you change the default command and state topics, then all the code here must also be modified to use those same topics.

|File Name|Contents / Use
|---------|--------|
|automation_scripts.yaml|Example automations and scripts related to controlling the receiver
|dashboard.yaml|YAML to create the sample dashboard shown below
|helpers.yaml|Helper entities, such as input numbers that can be used in a dashboard
|mqtt_entities.yaml|Entites such as sensors and switches for the FM receiver

#### Sample Dashboard
![HA_Dashboard_Largel](https://github.com/Resinchem/FM-Receiver-MQTT/assets/55962781/fa167b4f-53aa-4f5b-b0d4-8d47dc2886cb)

This is just a sample dashboard.  This version also requires two custom components, installed through the Home Assistant Community store (HACS).  These are listed in the comments at the top of the YAML file.

Automations, scripts and helpers can also be created using the Home Assistant UI Editor, but as of the time of publication MQTT entities can only be created via YAML.

Please see the [Wiki](https://github.com/Resinchem/FM-Receiver-MQTT/wiki) for more details on MQTT and MQTT topics.
