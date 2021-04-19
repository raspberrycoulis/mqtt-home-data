# MQTT Home Data #

A repository that contains a variety of code examples, primarily in Python or C++ that run, primarily on a Raspberry Pi or an ESP8266 variant (e.g. Node MCU or Wemo D1 Mini) as a way of monitoring various environmental data before publishing the data via MQTT (in my case, to an Influx DB via Telegram, which is then represented visually in a Grafana dashboard all run via Docker).

### What sensors are used? ###

The list below may grow over time, but currently:

* BME280 (for temperature, humidity, pressure and altitude)
* MCP9808 (for high accuracy temperature)
* TSL2561 (for luminence (light levels))
* MS430 by [Metriful](https://www.metriful.com/) (for temperature, humidity, pressure, air quality, sound levels, luminence and air particulates)
* [Enviro pHAT](https://shop.pimoroni.com/products/enviro-phat) (for temperature, pressure, luminence, RGB light values and movement)
* Si7021 (for temperature and humidity)

Additionally, a small OLED display is used in some to display data, as well as the [Micro Dot pHAT from Pimoroni](https://shop.pimoroni.com/products/microdot-phat) for the Raspberry Pi for another.

### Recommended case for ESP8266 + sensor(s) ###

One key problem that many projects have is finding a suitable case to house your ESP8266 board and sensor(s) in, or involves 3D printing something. Fortunately, there are some cases already available that are ideal for simple projects such as this. 

I personally use the [Hammond 1551V3WH Miniature Plastic Enclosure Vented 60x60x20mm White](https://www.hammfg.com/electronics/small-case/plastic/1551v), which fits my Wemo D1 Mini and applicable sensor in perfectly, plus is vented so allows better circulation on the sensors (with exception of the TSL2561 as this still needs to be able to "see" the light, so may need to be mounted externally or with a hole cut to allow light ingress.). If using a Node MCU or larger board, you can purchase a large dimenions case to accommodate, but I would always recommend the Wemo D1 Mini due to the size.

![Hammond Vented Enclosure](https://github.com/raspberrycoulis/mqtt-home-data/blob/master/hammond-case.png?raw=true)

The cases are also found in abundance on eBay and other reputable electronic specialists and include screw mount holes for the sensors (you will need some 4mm long M2 or M2.5 screws to mount the sensors - also available on eBay). I then mount the Wemo D1 Mini with some hot glue and then cut away a few of the vents in the case to allow USB access for power.

### Requirements ###

#### For data visualisation ####

Included is a `docker-compose.yml` file that can be used to get up-and-running quickly. This is optimised for use on Raspberry Pi, which will act as your "receiver" for all data. There are two seperate configuration files, `mosquitto.conf` and `telegraf.conf` which need to be used by the applicable Docker containers to work. **NOTE - This is not fully tested, so there may be some minor tweaks neeeded to get it to work on your setup! Most likely issues are around directory paths and the two configuration files named above!**. I currently run my "receiver" on my Synology NAS, but there are only a few differences between that and on the Pi.

* A MQTT broker - i.e. Eclipse Mosquitto ([Docker Hub](https://hub.docker.com/_/eclipse-mosquitto))
* InfluxDB - to store data sent by the MQTT broker ([Docker Hub](https://hub.docker.com/r/hypriot/rpi-influxdb))
* Telegraf - to translate data between MQTT and InfluxDB ([Docker Hub](https://hub.docker.com/_/telegraf))
* Grafana - to visualise your data in various forms ([Docker Hub](https://hub.docker.com/r/grafana/grafana))

**IMPORTANT NOTE - Running InfluxDB on a Raspberry Pi can quickly wear out a micro SD card due to the read / write activity involved, so it is recommended that the InfluxDB is contained on a separate device, such as a USB drive / HDD, or even in the cloud / on a NAS. If your SD card stops working faster than normal, then you have been warned!**

#### For Raspberry Pi (Python) ####

* Python PIP and Python3 PIP (`sudo apt-get update && sudo apt-get install python-pip python3-pip -y`)
* SMBUS (`sudo pip install smbus && sudo pip3 install smbus`)
* Paho MQTT (`sudo pip install paho-mqtt` or `sudo pip3 install paho-mqtt`)
* For the Si7021 sensor (`sudo pip3 install adafruit-circuitpython-si7021`)
* The Python BME280 library is included as `bme280.py` so this file must be in the same location as any Python code that includes the BME280 sensor to work

#### For ESP8266 (C++ via Arduino) ####

* `ESP8266WiFi.h` - to connect ESP8266 to WiFi ([GitHub repository](https://github.com/esp8266/Arduino))
* `Adafruit_BME280.h` - Adafruit's BME280 sensor library ([GitHub repository](https://github.com/adafruit/Adafruit_BME280_Library))
* `Adafruit_MCP9808.h` - Adafruit's MCP9808 sensor library ([GitHub repository](https://github.com/adafruit/Adafruit_MCP9808_Library))
* `Adafruit_TSL2561_U.h` - Adafruit's TSL2561 unified sensor library ([GitHub repository](https://github.com/adafruit/Adafruit_TSL2561))
* `SSD1306Wire.h` - ThingPulse's SSD1306 library for the OLED display ([GitHub repository](https://github.com/ThingPulse/esp8266-oled-ssd1306))
* `PubSubClient.h` - for MQTT publishing ([GitHub repository](https://github.com/knolleary/pubsubclient))
* `WiFiManager.h` - for configuring WiFi OTA ([GitHub repository](https://github.com/tzapu/WiFiManager))
* `ArduinoJson.h` - to send JSON data to MQTT ([GitHub repository](https://github.com/bblanchon/ArduinoJson))

### How data is reported ###

For convenience, the preferred method for sending data is done using the JSON format. Using JSON to send the data to the MQTT broker allows you to use other IoT related accessories, such as the [iOS app EasyMQTT](https://www.easymqtt.app/), to create widgts on your iOS device. All code examples that utilise JSON for this purposes include `json` in their title - for example, `esp8266-bme280-json-wifimanager-custom-mqtt.ino`.

JSON data is sent to the MQTT broker in the following format:

```json
{
  client_id : {
    "temperature" : 22.03,
    "humidity" : 24.26855,
    "pressure" : 1021.572,
    "altitude" : 110.0237
  },
  "device" : client-id,
  "sensor" : "BME280",
  "level" : level
}
```

Certain parameters are configurable during the setup of the sketch, for example `client_id` and `level`, so taking the above example with the `client_id = "garage"` and `level = downstairs`, the JSON payload would be:

```json
{
  "garage" : {
    "temperature" : 22.03,
    "humidity" : 24.26855,
    "pressure" : 1021.572,
    "altitude" : 110.0237
  },
  "device" : "garage",
  "sensor" : "BME280",
  "level" : "garage"
}
```

This makes it easier to identify and pull out data when using Grafana and / or EasyMQTT for visualisation purposes. Naturally, all dynamic variables, such as `temperature`, `humidity`, `pressure` etc., are updated once a sensor reading has been taken.

#### Sketch naming ####

ESP8266 sketches are named according to what equipment is used and how data is sent. For example:

* `esp8266-bme280-json-oled-wifimanager-custom-mqtt.ino` means:
  * (`bme280`) - BME280 sensor is used
  * (`json`) - Data is JSON formatted
  * (`oled`) - The OLED display is used
  * (`wifimanager`) - WiFiManager is used, allowing you to connect to the ESP8266's WiFi access point and configure your WiFi easily
  * (`custom`) - Uses the custom parameter capabilities within WiFiManager to set other parameters such as `channel`, `client_id`, `level` and where applicable, temperature calibration
  * (`mqtt`) - Sends data via MQTT - probably not needed now as MQTT is used in all, but hey-ho

### Configuring WiFi and setting custom parameters (ESP8266 only) ###

Sketches that use `WiFiManager` and `custom` tags allow you to configure additional settings within the sketch without having to program them via Arduino - for example:

* Setting a static IP address
* Setting MQTT broker IP / URL
* Choosing a client ID for the device
* Setting the level at which the sensor is on - e.g. upstairs, downstairs, outside - useful for segmenting data later on
* Temperature calibration - see below for more info on this

Once the sketch has been uploaded to your ESP8266, the device will enter AP (or Access Point) mode, broadcasting a WiFi network that is typically `ESP-XXXXX` where `XXXXX` is randomly generated. Simply connect to the WiFi network (using a smart phone / tablet works best for the captive portal), then provide the details in the login portal that appears after a few seconds. Once complete, hit save and the device will reboot and connect to your chosen WiFi network. If that WiFi network can no longer be found, the device will re-enter AP mode again, allowing you to reconfigure the network settings.

#### Temperature calibration (ESP8266 only) ####

Due to some quirks in how some sensors operate - e.g. self-heating due to electrical currents etc., - you may find that default readings are too high by several degrees. Thankfully, I have included a temperature calibration setting that allows you to set a value that is **subtracted** from the sensor reading before it is published to your MQTT broker. You simply enter the value you want to subtract when configuring the device initially as per above. I found this process to be a matter of trial and error, by comparing the values reported by the ESP8266 device with that of a known value (i.e. use a "dumb" thermometer to capture the reading and then compare to the one reported by the ESP8266) then go from there. I found that BME280 sensor readings needed calibrating by around 5.8 to 4.0 degrees to report more accurate readings.

### Acknowledgements ###

The code involving the MS430 sensor from Metriful is based on [their GitHub repository](https://github.com/metriful/sensor), and adapated by me to send data via MQTT.

Various sketches shown in the requirements above for ESP8266 devices are also used.