#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# This script will pull data from the Si7021 sensor and send it to a MQTT broker.

import time
import datetime
import sys
import json
try:
    import smbus
    import paho.mqtt.client as mqtt
    import adafruit_si7021
    import board
except ImportError:
    exit('This script requires the Adafruit Si7021 library, SMBUS and Paho MQTT\nInstall with: sudo pip3 install adafruit-circuitpython-si7021 paho-mqtt smbus')

# Initialise the Si7021 sensor
sensor = adafruit_si7021.SI7021(board.I2C())

# MQTT details - Update accordingly
brokerAddress = "192.168.1.24"  # IP or URL of your MQTT broker
clientName = "EnviroPi"         # The name your client reports to MQTT broker
channel = "homedev"             # The channel the MQTT data is published to
room = "living-room"            # The room your sensor is in - i.e. living-room
zone = "downstairs"             # The zone your sensor is in - i.e. upstairs
sensor = "si7021"

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

# Main code
while True:
    try:
        # Get the readings from the Si7021 sensor
        temperature = sensor.temperature
        humidity = sensor.relative_humidity
        # Sort the data for JSON - variables set earlier used here
        ## New structure (UNTESTED)
        raw_mqtt_data = {
            zone : {
                room : {
                    sensor : {
                        "temperature" : temperature,
                        "humidity" : humidity
                    },
                },
            },
        }

        ## Previous structure
        #raw_mqtt_data = {
        #    room : {
        #        "temperature" : temperature,
        #        "humidity" : humidity
        #    },
        #    "device" : room,
        #    "sensor" : "Si7021",
        #    "level" : zone
        #}
        JSON_mqtt_data = json.dumps(raw_mqtt_data)
        # Now try sending the data to MQTT broker
        if temperature is not None and humidity is not None:
            try:
                # Send data to MQTT
                now = datetime.datetime.now()
                client.publish(channel, JSON_mqtt_data,1,true)
                sys.stdout.flush()
            except Exception:
                # Error
                print ("Error while sending to MQTT broker")
                sys.stdout.flush()
            else:
                # Success!
                print("Data sent to MQTT broker " + str(brokerAddress) + " at " + (now.strftime("%H:%M:%S on %d/%m/%Y")))
                sys.stdout.flush()

        # Sleep and then repeat
        time.sleep(period)

    # Allow a graceful exit if run manually
    except (KeyboardInterrupt, SystemExit):
        sys.exit("Goodbye!")
        pass