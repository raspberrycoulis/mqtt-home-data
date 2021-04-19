#!/usr/bin/env python
# -*- coding: utf-8 -*-

import bme280
import paho.mqtt.client as mqtt
import time
import datetime
import sys
import httplib, urllib
import json
from microdotphat import write_string, set_decimal, clear, show, set_brightness

# MQTT details - Update accordingly
brokerAddress = "192.168.1.24"  # IP or URL of your MQTT broker
clientName = "PiZeroThomas"         # The name your client reports to MQTT broker
channel = "homedev"             # The channel the MQTT data is published to
room = "thomas-room"            # The room your sensor is in - i.e. living-room
zone = "upstairs"             # The zone your sensor is in - i.e. upstairs

# Global brightness on Microdot pHAT
set_brightness(0.25)

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

# Run the main code in a loop
while True:
    try:
        # Get readings from the BME280 sensor
        temperature,pressure,humidity = bme280.readBME280All()
        # Sort the data for JSON - variables set earlier used here
        raw_mqtt_data = {
            room : {
                "temperature" : temperature,
                "humidity" : humidity,
                "pressure" : pressure
            },
            "device" : room,
            "sensor" : "BME280",
            "level" : zone
        }
        JSON_mqtt_data = json.dumps(raw_mqtt_data)
        # Now try sending the data to MQTT broker and dusplay on Micro Dot pHAT
        if temperature is not None and pressure is not None and humidity is not None:
            try:
                now = datetime.datetime.now()
                client.publish(channel, JSON_mqtt_data)
                print("Data sent to MQTT broker " + str(brokerAddress) + " at " + (now.strftime("%H:%M:%S on %d/%m/%Y")))
                sys.stdout.flush()
                # Microdot pHAT stuff
                zzz = 20
                # Display the time
                clear()
                t = datetime.datetime.now()
                write_string(t.strftime('%H:%M'), kerning=False)
                show()
                time.sleep(zzz)
                # Display the temperature
                clear()
                write_string( "%.1f" % temperature + "C", kerning=False)
                show()
                time.sleep(zzz)
                # Uncomment to display pressure if needed
                #clear()
                #write_string( "%.0f" % pressure + "hPa", kerning=False)
                #show()
                #time.sleep(zzz)
                # Display the humidity
                clear()
                write_string( "%.0f" % humidity + "% RH", kerning=False)
                show()
                time.sleep(zzz)
            except Exception:
                # Error
                print("Error while sending to MQTT broker")
                sys.stdout.flush()

    except (KeyboardInterrupt, SystemExit):
        sys.exit("Goodbye!")
        pass