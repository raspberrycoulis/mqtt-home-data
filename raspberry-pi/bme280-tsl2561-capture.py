#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This script will pull data from the BME280 and TSL2561 sensors and send it to a MQTT broker.

import time
import smbus
import bme280
import paho.mqtt.client as mqtt

# Initialise the TSL2561 sensor
bus = smbus.SMBus(1)

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "EnviroPi"         # Update accordingly

# Define the time between sending data to MQTT broker - default is 60 seconds
period = 60

# Get ambient light level from the TSL2561 sensor
def get_light():
    bus.write_byte_data(0x39, 0x00 | 0x80, 0x03)
    bus.write_byte_data(0x39, 0x01 | 0x80, 0x02)
    time.sleep(0.5)
    full_data = bus.read_i2c_block_data(0x39, 0x0C | 0x80, 2)
    ir_data = bus.read_i2c_block_data(0x39, 0x0E | 0x80, 2)
    full_spectrum = full_data[1] * 256 + full_data[0]
    infrared = ir_data[1] * 256 + ir_data[0]
    visible = full_spectrum - infrared
    return visible

# Main code
def run():
  while True:
    # Get the readings from the BME280 sensor
    temperature,pressure,humidity = bme280.readBME280All()
    # Get the lux level from the TSL2561 sensor
    lux = get_light()
    if temperature is not None and pressure is not None and humidity is not None and lux is not None:
        try:
            # Send data to MQTT
            client = mqtt.Client(clientName)
            client.connect(brokerAddress)
            client.publish("sensors", "temperature,room=living-room-bme280 value=" +str(temperature))
            client.publish("sensors", "humidity,room=living-room-bme280 value=" +str(humidity))
            client.publish("sensors", "pressure,room=living-room-bme280 value=" +str(pressure))
            client.publish("sensors", "lux,room=living-room-tsl2561 value=" +str(lux))
        except Exception:
          # Process exception here
          print ("Error while sending to MQTT broker")
    else:
        print ("Failed to get reading. Try again!")

    # Sleep some time
    time.sleep(period)

run()