# MQTT Home Data #

A repository that contains a variety of code examples, primarily in Python or C++ that run, primarily on a Raspberry Pi or an ESP8266 variant (e.g. Node MCU or Wemo D1 Mini) as a way of monitoring various environmental data before publishing the data via MQTT (in my case, to an Influx DB via Telegram, which is then represented visually in a Grafana dashboard all run via Docker).

### What sensors are used? ###

The list below may grow over time, but currently:
* BME280 (for temperature, humidity and pressure)
* MCP9808 (for high accuracy temperature)
* TSL2561 (for luminence)
* MS430 (for temperature, humidity, pressure, air quality, sound levels, luminence and air particulates)
* [Enviro pHAT](https://shop.pimoroni.com/products/enviro-phat) (for temperature, pressure, luminence, RGB light values and movement)

Additionally, a small OLED display is used in some to display data.

### Requirements ###

To be added to, but currently:
* Paho MQTT (`sudo pip install paho-mqtt` or `sudo pip3 install paho-mqtt`)
* Various Arduino sketches (will update in due course)

### Acknowledgements ###

The code involving the MS430 sensor from Metriful is based on [their GitHub repository](https://github.com/metriful/sensor), and adapated by me to send data via MQTT.