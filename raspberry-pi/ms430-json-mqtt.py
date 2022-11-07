# -*- coding: utf-8 -*-

from new_sensor_functions import *
import paho.mqtt.client as mqtt
import time
import datetime
import json

#########################################################
# USER-EDITABLE SETTINGS

# How often to read data (every 3, 100, 300 seconds)
cycle_period = CYCLE_PERIOD_3_S

# Which particle sensor, if any, is attached
# (PARTICLE_SENSOR_X with X = PPD42, SDS011, or OFF)
particleSensor = PARTICLE_SENSOR_OFF

# How to print the data: If print_data_as_columns = True,
# data are columns of numbers, useful to copy/paste to a spreadsheet
# application. Otherwise, data are printed with explanatory labels and units.
print_data_as_columns = False

# MQTT details
brokerAddress = "192.168.1.24"  # Update accordingly
clientName = "MS430-Pi"         # Update accordingly
channel = "homedev"             # Update accordingly
room = "office-ms430"           # Update accordingly
zone = "upstairs"               # Update accordingly

# Define the time between sending data to MQTT broker - default is 60 seconds
period = 60

# END OF USER-EDITABLE SETTINGS
#########################################################

# Set up the GPIO and I2C communications bus
(GPIO, I2C_bus) = SensorHardwareSetup()

# Apply the chosen settings to the MS430
I2C_bus.write_i2c_block_data(i2c_7bit_address, PARTICLE_SENSOR_SELECT_REG, [PARTICLE_SENSOR])
I2C_bus.write_i2c_block_data(i2c_7bit_address, CYCLE_TIME_PERIOD_REG, [cycle_period])

# Unicode symbol strings
CELSIUS_SYMBOL = "\u00B0C"
FAHRENHEIT_SYMBOL = "\u00B0F"
SDS011_CONC_SYMBOL = "\u00B5g/m\u00B3" # micrograms per cubic meter
SUBSCRIPT_2 = "\u2082"
OHM_SYMBOL = "\u03A9"

#########################################################

print("Entering cycle mode and waiting for data. Press ctrl-c to exit.")

I2C_bus.write_byte(i2c_7bit_address, CYCLE_MODE_CMD)

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

while (True):
  try:
    # Get the time
    now = datetime.datetime.now()
    # Wait for the next new data release, indicated by a falling edge on READY
    while (not GPIO.event_detected(READY_pin)):
      sleep(0.05)

    # Temperature and humidity data
    air_data = get_air_data(I2C_bus)

    # Get air quality data
    air_quality_data = get_air_quality_data(I2C_bus)

    # Get light data
    light_data = get_light_data(I2C_bus)

    # Get sound data
    sound_data = get_sound_data(I2C_bus)

    # Formatting for JSON
    temperature_data = air_data['T']
    humidity_data = air_data['H_pc']
    pressure_data = air_data['P_Pa']/100
    lux_data = light_data['illum_lux']
    aq_index = air_quality_data['AQI']
    aq_accuracy = air_quality_data['AQI_accuracy']
    bvoc_data = air_quality_data['bVOC']
    co2_data = air_quality_data['CO2e']
    gas_data = air_data['G_ohm']
    peak_amp_data = sound_data['peak_amp_mPa']
    dba_data = sound_data['SPL_dBA']

    # Sort the data for JSON - variables set earlier used here
    raw_mqtt_data = {
        room : {
            "ms430" : {
            "sensor" : "MS430",
            "temperature" : temperature_data,
            "humidity" : humidity_data,
            "pressure" : pressure_data,
            "lux" : lux_data,
            "airquality" : aq_index,
            "airqual_accuracy" : aq_accuracy,
            "breath-voc" : bvoc_data,
            "est-co2" : co2_data,
            "gas-resistance" : gas_data,
            "peak-amplitude" : peak_amp_data,
            "dba" : dba_data
            },
        },
        "device" : room,
        "level" : zone
    }
    JSON_mqtt_data = json.dumps(raw_mqtt_data)
    # Now try sending the data to MQTT broker
    if temperature_data is not None and humidity_data is not None and pressure_data is not None and lux_data is not None and aq_index is not None and aq_accuracy is not None and bvoc_data is not None and co2_data is not None and gas_data is not None and peak_amp_data is not None and dba_data is not None:
          try:
            # Send data to MQTT
            now = datetime.datetime.now()
            client.publish(channel, JSON_mqtt_data, qos=1, retain=True)
            sys.stdout.flush()
          except Exception:
            # Error
            print ("Error while sending to MQTT broker")
            sys.stdout.flush()
          else:
            # Success!
            print ("Data sent to MQTT broker " + str(brokerAddress) + " at " + (now.strftime("%H:%M:%S on %d/%m/%Y")))
            sys.stdout.flush()
    time.sleep(period)

    if (particleSensor != PARTICLE_SENSOR_OFF):
      raw_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, PARTICLE_DATA_READ, PARTICLE_DATA_BYTES)
      particle_data = extractParticleData(raw_data, particleSensor)
      writeParticleData(None, particle_data, print_data_as_columns)

    if print_data_as_columns:
      print("")
    else:
      print("-------------------------------------------")
  
  except (KeyboardInterrupt, SystemExit):
    sys.exit("Goodbye!")
    pass
