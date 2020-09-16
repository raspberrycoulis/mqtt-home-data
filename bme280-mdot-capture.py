#!/usr/bin/env python
# -*- coding: utf-8 -*-

import bme280
import paho.mqtt.client as mqtt
import time
import datetime
import sys
import threading
import httplib, urllib
from microdotphat import write_string, set_decimal, clear, show

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "PiZeroThomas"     # Update accordingly

# Sends data to MQTT
def sendData():
    while True:
        client = mqtt.Client(clientName)
        client.connect(brokerAddress)
        client.publish("sensors", "temp,room=thomas-room value=" +str(temperature))
        client.publish("sensors", "humidity,room=thomas-room value=" +str(humidity))
        client.publish("sensors", "pressure,room=thomas-room value=" +str(pressure))
        time.sleep(60)

# Experimental! Pushover notifications - should work
# Replace APP_TOKEN and USER_TOKEN where applicable
def pushover():
    conn = httplib.HTTPSConnection("api.pushover.net:443")
    conn.request("POST", "/1/messages.json",
      urllib.urlencode({
        "token": "APP_TOKEN",
        "user": "USER_TOKEN",
        "html": "1",
        "title": "High temperature!",
        "message": "It is "+str(temperature)+ "°C in the nursery!",
        "url": "https://dashboard.url.goes.here",
        "url_title": "View Grafana dashboard",
        "sound": "siren",
      }), { "Content-type": "application/x-www-form-urlencoded" })
    conn.getresponse()

# Display stats on the Micro Dot pHAT
def microdot():
    clear()
    write_string( "%.1f" % temperature + "C", kerning=False)
    show()
    time.sleep(5)
    # Uncomment to display pressure if needed
    #clear()
    #write_string( "%.0f" % pressure + "hPa", kerning=False)
    #show()
    #time.sleep(5)
    clear()
    write_string( "%.0f" % humidity + "% RH", kerning=False)
    show()
    time.sleep(5)

try:
    # Get the first reading from the BME280 sensor
    temperature,pressure,humidity = bme280.readBME280All()
    # Start the sendData function as a thread so it works in the background
    sendData_thread = threading.Thread(target=sendData)
    sendData.daemon = True
    sendData_thread.start()
    # Run a loop to collect data and display it on the Micro Dot pHAT
    # and send notifications via Pushover if it is too hot
    while True:
        temperature,pressure,humidity = bme280.readBME280All()
        if temperature >= 26:
            pushover()
            microdot()
            pass
        else:
            pass
        microdot()

except (KeyboardInterrupt, SystemExit):
    sys.exit()
    pass