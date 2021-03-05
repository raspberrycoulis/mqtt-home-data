#!/usr/bin/env python
# -*- coding: utf-8 -*-

################################################################################
## Note! This file runs the microdot() function in a thread and the sendData() ##
## function in the loop - trying to understand what causes the program to     ##
## crash, so hoping this helps.                                               ##
################################################################################

import bme280
import paho.mqtt.client as mqtt
import time
import datetime
import sys
import threading
import httplib, urllib
from microdotphat import write_string, set_decimal, clear, show, set_brightness

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "PiZeroThomas"     # Update accordingly

# Global brightness on Microdot pHAT
set_brightness(0.25)

# Sends data to MQTT
def sendData():
    # Get the first reading from the BME280 sensor
    temperature,pressure,humidity = bme280.readBME280All()
    now = datetime.datetime.now()
    client = mqtt.Client(clientName)
    client.connect(brokerAddress)
    client.publish("sensors", "temperature,room=thomas-room,floor=upstairs value=" +str(temperature))
    client.publish("sensors", "humidity,room=thomas-room,floor=upstairs value=" +str(humidity))
    client.publish("sensors", "pressure,room=thomas-room,floor=upstairs value=" +str(pressure))
    print("Data sent at: " +(now.strftime("%H:%M:%S on %d/%m/%Y")))
    sys.stdout.flush()
    time.sleep(60)

# Display stats on the Micro Dot pHAT
def microdot():
    while True:
        # Get the first reading from the BME280 sensor
        temperature,pressure,humidity = bme280.readBME280All()
        # Sleep variable for switching displays
        zzz = 10
        # Display the time
        clear()
        t = datetime.datetime.now()
        write_string(t.strftime('%H:%M'), kerning=False)
        show()
        #print("Micro Dot: Time showing")
        #sys.stdout.flush()
        time.sleep(zzz)
        # Display the temperature
        clear()
        write_string( "%.1f" % temperature + "C", kerning=False)
        show()
        #print("Micro Dot: Temperature showing")
        #sys.stdout.flush()
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
        #print("Micro Dot: Humidity showing")
        #sys.stdout.flush()
        time.sleep(zzz)

try:
    # Get the first reading from the BME280 sensor
    #temperature,pressure,humidity = bme280.readBME280All()
    # Start the microdot function as a thread so it works in the background
    microdot_thread = threading.Thread(target=microdot)
    microdot.daemon = True
    microdot_thread.start()
    # Run a loop to collect data and display it on the Micro Dot pHAT
    while True:
        sendData()

except (KeyboardInterrupt, SystemExit):
    sys.exit()
    pass