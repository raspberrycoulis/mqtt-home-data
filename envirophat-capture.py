#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This script will pull data from the BME280 sensor and send it to a MQTT broker.

import time
from envirophat import light, weather
import paho.mqtt.client as mqtt

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "OfficePi"         # Update accordingly

# Define the time between sending data to MQTT broker - default is 60 seconds
period = 60

def run():
  while True:
    # Get the first reading from the Enviro pHAT
    temperature,pressure,lux = weather.temperature() -5.8, weather.pressure()/100, light.light()
    if temperature is not None and pressure is not None and lux is not None:
        try:
            # Send data to MQTT
            client = mqtt.Client(clientName)
            client.connect(brokerAddress)
            client.publish("sensors", "temp,room=office-enviro value=" +str(temperature))
            client.publish("sensors", "pressure,room=office-enviro value=" +str(pressure))
            client.publish("sensors", "lux,room=office-enviro value=" +str(lux))
        except Exception:
          # Process exception here
          print ("Error while sending to MQTT broker")
    else:
        print ("Failed to get reading. Try again!")

    # Sleep some time
    time.sleep(period)

run()