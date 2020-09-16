# -*- coding: utf-8 -*-

from sensor_functions import *
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

# END OF USER-EDITABLE SETTINGS
#########################################################

# Set up the GPIO and I2C communications bus
(GPIO, I2C_bus) = SensorHardwareSetup()

# Apply the chosen settings
if (particleSensor != PARTICLE_SENSOR_OFF):
  I2C_bus.write_i2c_block_data(i2c_7bit_address, PARTICLE_SENSOR_SELECT_REG, [particleSensor])
I2C_bus.write_i2c_block_data(i2c_7bit_address, CYCLE_TIME_PERIOD_REG, [cycle_period])

#########################################################

print("Entering cycle mode and waiting for data. Press ctrl-c to exit.")

I2C_bus.write_byte(i2c_7bit_address, CYCLE_MODE_CMD)

while (True):

  # Wait for the next new data release, indicated by a falling edge on READY
  while (not GPIO.event_detected(READY_pin)):
    sleep(0.05)

  # Humidity data
  humidity_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, H_READ, H_BYTES)
  humidity_integer = humidity_data[0]
  humidity_fraction = humidity_data[1]

  # Temperature data
  temperature_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, T_READ, T_BYTES)
  temperature_positive_integer = temperature_data[0] & TEMPERATURE_VALUE_MASK
  temperature_fraction = temperature_data[1]

  # Gas sensor data - not working reliably
  #gas_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, G_READ, G_BYTES)
  #gas_integer = gas_data[0]

  # Air quality data - not working reliably
  #aq_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, AQI_READ, AQI_BYTES)
  #aq_integer_lsb = aq_data[0]
  #aq_integer_msb = aq_data[1]
  #aq_fraction = aq_data[2]

  # Illuminance data
  lux_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, ILLUMINANCE_READ, ILLUMINANCE_BYTES)
  lux_integer_lsb = lux_data[0]
  lux_integer_msb = lux_data[1]
  lux_fraction = lux_data[2]

  # White light data - not really that useful
  #light_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, WHITE_LIGHT_READ, WHITE_BYTES)
  #white_light = light_data[0]

  # Sound data
  sound_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, SPL_READ, SPL_BYTES)
  sound_integer = sound_data[0]
  sound_fraction = sound_data[1]

  # Send data to MQTT
  client = mqtt.Client(clientName)
  client.connect(brokerAddress)
  print("Humidity = " + str(humidity_integer) + "." + str(humidity_fraction) + " %")
  client.publish("sensors", "humidity,room=office-ms430 value=" + str(humidity_integer) + "." + str(humidity_fraction))
  print("Temperature = " + str(temperature_positive_integer) + "." + str(temperature_fraction) + " °C")
  client.publish("sensors", "temperature,room=office-ms430 value=" + str(temperature_positive_integer) + "." + str(temperature_fraction))
  print("Lux = " + str(lux_integer_lsb) + str(lux_integer_msb) + "." + str(lux_fraction) + " lux")
  client.publish("sensors", "lux,room=office-ms430 value=" + str(lux_integer_lsb) + str(lux_integer_msb) + "." + str(lux_fraction))
  #print("Gas resistance = " + str(gas_integer) + " ohm")
  #client.publish("sensors", "gas-resistance,room=office-ms430 value=" + str(gas_integer))
  #print("Air quality index = " + str(aq_integer_lsb) + str(aq_integer_msb) + "." + str(aq_fraction))
  #client.publish("sensors", "air-qual,room=office-ms430 value=" + str(aq_integer_lsb) + str(aq_integer_msb) + "." + str(aq_fraction))
  #print("White light = " + str(white_light))
  #client.publish("sensors", "white-light,room=office-ms430 value=" + str(white_light))
  print("Sound pressure = " + str(sound_integer) + "." + str(sound_fraction) + " dBA")
  client.publish("sensors", "sound,room=office-ms430 value=" + str(sound_integer) + "." + str(sound_fraction)) 
  print("Data sent to MQTT broker, " + str(brokerAddress) + ".")

  if (particleSensor != PARTICLE_SENSOR_OFF):
    raw_data = I2C_bus.read_i2c_block_data(i2c_7bit_address, PARTICLE_DATA_READ, PARTICLE_DATA_BYTES)
    particle_data = extractParticleData(raw_data, particleSensor)
    writeParticleData(None, particle_data, print_data_as_columns)

  if print_data_as_columns:
    print("")
  else:
    print("-------------------------------------------")