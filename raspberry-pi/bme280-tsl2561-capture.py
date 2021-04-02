#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This script will pull data from the BME280 and TSL2561 sensors and send it to a MQTT broker.

import time
import datetime
import sys
import smbus
import bme280
import paho.mqtt.client as mqtt

# Initialise the TSL2561 sensor
bus = smbus.SMBus(1)

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "EnviroPi"         # Update accordingly
room = "living-room"            # Update accordingly
zone = "downstairs"             # Update accordingly

# Define the time between sending data to MQTT broker - default is 60 seconds
period = 60

# For MQTT connections
def on_disconnect(client, userdata, rc):
    if rc!=0:
      print("MQTT disconnected. Will auto-reconnect...")
      sys.stdout.flush()

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.connected_flag = True
        print("Connected to MQTT broker as {}.".format(clientName))
        sys.stdout.flush()
    else:
        print("Connection error! Result code: {}".format(rc))
        sys.stdout.flush()

mqtt.Client.connected_flag = False
client = mqtt.Client(clientName)
client.on_connect = on_connect
client.on_disconnect = on_disconnect
client.loop_start()
client.connect(brokerAddress)
while not client.connected_flag:
    print("Connecting...")
    time.sleep(1)

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
while True:
    try:
         # Get the readings from the BME280 sensor
        temperature,pressure,humidity = bme280.readBME280All()
        # Get the lux level from the TSL2561 sensor
        lux = get_light()
        if temperature is not None and pressure is not None and humidity is not None and lux is not None:
            try:
                # Send data to MQTT
                now = datetime.datetime.now()
                client.publish("sensors", "temperature,room=" + str(room) + "-bme280,floor=" + str(zone) + " value=" + str(temperature))
                client.publish("sensors", "humidity,room=" + str(room) + "-bme280,floor=" + str(zone) + " value=" + str(humidity))
                client.publish("sensors", "pressure,room=" + str(room) + "-bme280,floor=" + str(zone) + " value=" + str(pressure))
                client.publish("sensors", "lux,room=" + str(room) + "-tsl2561,floor=" + str(zone) + " value=" + str(lux))
                sys.stdout.flush()
            except Exception:
                # Process exception here
                print ("Error while sending to MQTT broker")
            else:
                print("Data sent to MQTT broker " + str(brokerAddress) + " at " + (now.strftime("%H:%M:%S on %d/%m/%Y")))

        # Sleep some time
        time.sleep(period)

    except (KeyboardInterrupt, SystemExit):
        sys.exit("Goodbye!")
        pass