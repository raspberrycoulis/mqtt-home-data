# -*- coding: utf-8 -*-

from new_sensor_functions import *
import paho.mqtt.client as mqtt
import time

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
room = "office-ms430"            # Update accordingly
zone = "upstairs"               # Update accordingly

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

    # Send data to MQTT
    print("Temperature = {:.1f} ".format(air_data['T_C']) + air_data['C_unit'])
    client.publish("sensors", "temperature,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.1f}".format(air_data['T']))
    print("Humidity = {:.1f} %".format(air_data['H_pc']))
    client.publish("sensors", "humidity,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.1f}".format(air_data['H_pc']))
    print("Pressure = " + str(air_data['P_Pa']/100) + " hPa")
    client.publish("sensors", "pressure,room=" + str(room) + ",floor=" + str(zone) + " value=" + str(air_data['P_Pa']/100))
    print("Lux = {:.2f} lux".format(light_data['illum_lux']))
    client.publish("sensors", "lux,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.2f}".format(light_data['illum_lux']))
    print("Air quality index = {:.1f}".format(air_quality_data['AQI']))
    client.publish("sensors", "airquality,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.1f}".format(air_quality_data['AQI']))
    print("Air quality accuracy = " + str(air_quality_data['AQI_accuracy']) + "/3")
    client.publish("sensors", "airquality-accuracy,room=" + str(room) + ",floor=" + str(zone) + " value=" + str(air_quality_data['AQI_accuracy']))
    print("Breath VOC = {:.2f} ppm".format(air_quality_data['bVOC']))
    client.publish("sensors", "bvoc,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.2f}".format(air_quality_data['bVOC']))
    print("Estimated CO" + SUBSCRIPT_2 + " = {:.1f} ppm".format(air_quality_data['CO2e']))
    client.publish("sensors", "co2,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.1f}".format(air_quality_data['CO2e']))
    print("Gas sensor resistance = " + str(air_data['G_ohm']) + " " + OHM_SYMBOL)
    client.publish("sensors", "gas-resistance,room=" + str(room) + ",floor=" + str(zone) + " value=" + str(air_data['G_ohm']))
    print("Peak amplitude = {:.2f} mPa".format(sound_data['peak_amp_mPa']))
    client.publish("sensors", "sound-peak-amp,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.2f}".format(sound_data['peak_amp_mPa']))
    print("A-weighted sound pressure = {:.1f} dBA".format(sound_data['SPL_dBA']))
    client.publish("sensors", "sound-decibels,room=" + str(room) + ",floor=" + str(zone) + " value=" + "{:.1f}".format(sound_data['SPL_dBA']))
    print("Data sent to MQTT broker, " + str(brokerAddress) + ".")
    time.sleep(60)

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