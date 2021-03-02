#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This script will pull data from the BME280 sensor and send it to a MQTT broker.

import time
import smbus
import bme280
import paho.mqtt.client as mqtt

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "EnviroPi"         # Update accordingly

# Define the time between sending data to MQTT broker - default is 60 seconds
period = 60

# Main code
def run():
  while True:
    # Get the readings from the BME280 sensor
    temperature,pressure,humidity = bme280.readBME280All()
    if temperature is not None and pressure is not None and humidity is not None:
        try:
            # Send data to MQTT
            client = mqtt.Client(clientName)
            client.connect(brokerAddress)
            client.publish("sensors", "temp,room=living-room-bme280 value=" +str(temperature))
            client.publish("sensors", "humidity,room=living-room-bme280 value=" +str(humidity))
            client.publish("sensors", "pressure,room=living-room-bme280 value=" +str(pressure))
        except Exception:
          # Process exception here
          print ("Error while sending to MQTT broker")
    else:
        print ("Failed to get reading. Try again!")

    # Sleep some time
    time.sleep(period)

run()