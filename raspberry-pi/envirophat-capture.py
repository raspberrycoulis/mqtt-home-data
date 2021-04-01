#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This script will pull data from the Enviro pHAT sensor and send it to a MQTT broker.

import time
import datetime
import sys
from envirophat import light, weather
import paho.mqtt.client as mqtt

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "OfficePi"         # Update accordingly
room = "office-enviro"          # Update accordingly
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

while True:
  try:
    # Get the time
    now = datetime.datetime.now()
    # Get the first reading from the Enviro pHAT
    temperature,pressure,lux = weather.temperature() -5.8, weather.pressure()/100, light.light()
    if temperature is not None and pressure is not None and lux is not None:
        try:
            # Send data to MQTT
            now = datetime.datetime.now()
            client.publish("sensors", "temperature,room=" + str(room) + ",floor=" + str(zone) + " value=" + str(temperature))
            client.publish("sensors", "pressure,room=" + str(room) + ",floor=" + str(zone) + " value=" + str(pressure))
            client.publish("sensors", "lux,room=" + str(room) + ",floor=" + str(zone) + " value=" + str(lux))
            print("Data sent to MQTT broker " + str(brokerAddress) + " at " + (now.strftime("%H:%M:%S on %d/%m/%Y")))
            sys.stdout.flush()
        except Exception:
          # Process exception here
          print ("Error while sending to MQTT broker")
    else:
        print ("Failed to get reading. Try again!")

    # Sleep some time
    time.sleep(period)

  except (KeyboardInterrupt, SystemExit):
    sys.exit("Goodbye!")
    pass