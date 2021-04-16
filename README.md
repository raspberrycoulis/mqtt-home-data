# MQTT Home Data #

A repository that contains a variety of code examples, primarily in Python or C++ that run, primarily on a Raspberry Pi or an ESP8266 variant (e.g. Node MCU or Wemo D1 Mini) as a way of monitoring various environmental data before publishing the data via MQTT (in my case, to an Influx DB via Telegram, which is then represented visually in a Grafana dashboard all run via Docker).

### What sensors are used? ###

The list below may grow over time, but currently:
* BME280 (for temperature, humidity and pressure)
* MCP9808 (for high accuracy temperature)
* TSL2561 (for luminence)
* MS430 (for temperature, humidity, pressure, air quality, sound levels, luminence and air particulates)
* [Enviro pHAT](https://shop.pimoroni.com/products/enviro-phat) (for temperature, pressure, luminence, RGB light values and movement)
* Si7021 (for temperature and humidity)

Additionally, a small OLED display is used in some to display data.

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
  * `ESP8266WiFi.h` - to connect ESP8266 to WiFi
  * `Adafruit_BME280.h` - Adafruit's BME280 sensor library ([GitHub repository](https://github.com/adafruit/Adafruit_BME280_Library))
  * `Adafruit_MCP9808.h` - Adafruit's MCP9808 sensor library ([GitHub repository](https://github.com/adafruit/Adafruit_MCP9808_Library))
  * `Adafruit_TSL2561_U.h` - Adafruit's TSL2561 unified sensor library ([GitHub repository](https://github.com/adafruit/Adafruit_TSL2561))
  * `SSD1306Wire.h` - ThingPulse's SSD1306 library for the OLED display ([GitHub repository](https://github.com/ThingPulse/esp8266-oled-ssd1306))
  * `PubSubClient.h` - for MQTT publishing ([GitHub repository](https://github.com/knolleary/pubsubclient))
  * `WiFiManager.h` - for configuring WiFi OTA ([GitHub repository](https://github.com/tzapu/WiFiManager))
  * `ArduinoJson.h` - to send JSON data to MQTT ([GitHub repository](https://github.com/bblanchon/ArduinoJson))

### Acknowledgements ###

The code involving the MS430 sensor from Metriful is based on [their GitHub repository](https://github.com/metriful/sensor), and adapated by me to send data via MQTT.

Various sketches shown in the requirements above for ESP8266 devices are also used.