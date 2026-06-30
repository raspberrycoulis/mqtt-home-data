#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
MS430 dual MQTT publisher.

Publishes:
1. Existing nested JSON payload to Synology NAS MQTT broker for InfluxDB/Grafana.
2. Home Assistant-friendly flat JSON payload to Home Assistant MQTT broker.
3. Home Assistant MQTT Discovery config to Home Assistant MQTT broker.
4. Home Assistant availability status to Home Assistant MQTT broker.

This version hard-codes MQTT settings for local-only use.
Do not commit this version to GitHub with credentials included.
"""

import json
import signal
import sys
import time
import datetime as dt

import paho.mqtt.client as mqtt

from new_sensor_functions import *


# ---------------------------------------------------------------------------
# Sensor settings
# ---------------------------------------------------------------------------

CYCLE_PERIOD = CYCLE_PERIOD_3_S
PARTICLE_SENSOR = PARTICLE_SENSOR_OFF


# ---------------------------------------------------------------------------
# NAS / legacy MQTT broker settings
# ---------------------------------------------------------------------------

NAS_MQTT_HOST = "REPLACE_ME"
NAS_MQTT_PORT = 1883
NAS_MQTT_USERNAME = None
NAS_MQTT_PASSWORD = None

LEGACY_MQTT_TOPIC = "homedev"
PUBLISH_LEGACY_PAYLOAD = True

RETAIN_LEGACY_STATE = False


# ---------------------------------------------------------------------------
# Home Assistant MQTT broker settings
# ---------------------------------------------------------------------------

HA_MQTT_HOST = "REPLACE_ME"
HA_MQTT_PORT = 1883
HA_MQTT_USERNAME = "REPLACE_ME"
HA_MQTT_PASSWORD = "REPLACE_ME"

PUBLISH_HA_PAYLOAD = True

LOCATION_ZONE = "upstairs"
LOCATION_ROOM = "office"
SENSOR_NAME = "ms430"

DEVICE_ID = f"{SENSOR_NAME}_{LOCATION_ROOM}".lower().replace(" ", "_")
DEVICE_NAME = f"MS430 {LOCATION_ROOM.title()}"

HA_BASE_TOPIC = f"home/{LOCATION_ZONE}/{LOCATION_ROOM}/{SENSOR_NAME}"
HA_STATE_TOPIC = f"{HA_BASE_TOPIC}/state"
HA_AVAILABILITY_TOPIC = f"{HA_BASE_TOPIC}/status"

DISCOVERY_PREFIX = "homeassistant"

RETAIN_DISCOVERY = True
RETAIN_HA_STATE = False
RETAIN_AVAILABILITY = True


# ---------------------------------------------------------------------------
# General publish settings
# ---------------------------------------------------------------------------

MQTT_QOS = 1
PUBLISH_PERIOD = 60
EXPIRE_AFTER = PUBLISH_PERIOD * 3


# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

def safe_round(value, digits=2):
    if value is None:
        return None

    try:
        return round(float(value), digits)
    except (TypeError, ValueError):
        return None


def now_iso():
    return dt.datetime.now().astimezone().isoformat(timespec="seconds")


def now_display():
    return dt.datetime.now().strftime("%H:%M:%S on %d/%m/%Y")


def build_payloads(i2c_bus):
    """
    Read the sensor once, then build both payload formats:
    - legacy nested JSON for NAS / InfluxDB / Grafana
    - flat JSON for Home Assistant
    """

    air_data = get_air_data(i2c_bus)
    air_quality_data = get_air_quality_data(i2c_bus)
    light_data = get_light_data(i2c_bus)
    sound_data = get_sound_data(i2c_bus)

    temperature = safe_round(air_data.get("T"), 2)
    humidity = safe_round(air_data.get("H_pc"), 2)

    pressure = None
    if air_data.get("P_Pa") is not None:
        pressure = safe_round(air_data.get("P_Pa") / 100, 2)

    illuminance = safe_round(light_data.get("illum_lux"), 2)
    air_quality_index = safe_round(air_quality_data.get("AQI"), 0)
    air_quality_accuracy = safe_round(air_quality_data.get("AQI_accuracy"), 0)
    breath_voc = safe_round(air_quality_data.get("bVOC"), 3)
    estimated_co2 = safe_round(air_quality_data.get("CO2e"), 0)
    gas_resistance = safe_round(air_data.get("G_ohm"), 0)
    peak_amplitude = safe_round(sound_data.get("peak_amp_mPa"), 2)
    sound_pressure = safe_round(sound_data.get("SPL_dBA"), 2)

    last_update = now_iso()

    legacy_payload = {
        LOCATION_ZONE: {
            LOCATION_ROOM: {
                SENSOR_NAME: {
                    "temperature": temperature,
                    "humidity": humidity,
                    "pressure": pressure,
                    "illuminance": illuminance,
                    "air_quality_index": air_quality_index,
                    "air_quality_accuracy": air_quality_accuracy,
                    "breath_voc": breath_voc,
                    "estimated_co2": estimated_co2,
                    "gas_resistance": gas_resistance,
                    "peak_amplitude": peak_amplitude,
                    "sound_pressure": sound_pressure,
                    "last_update": last_update,
                }
            }
        }
    }

    ha_payload = {
        "temperature": temperature,
        "humidity": humidity,
        "pressure": pressure,
        "illuminance": illuminance,
        "air_quality_index": air_quality_index,
        "air_quality_accuracy": air_quality_accuracy,
        "breath_voc": breath_voc,
        "estimated_co2": estimated_co2,
        "gas_resistance": gas_resistance,
        "peak_amplitude": peak_amplitude,
        "sound_pressure": sound_pressure,
        "last_update": last_update,
    }

    return legacy_payload, ha_payload


def payload_has_required_values(payload):
    required_keys = [
        "temperature",
        "humidity",
        "pressure",
        "illuminance",
        "air_quality_index",
        "air_quality_accuracy",
        "breath_voc",
        "estimated_co2",
        "gas_resistance",
        "peak_amplitude",
        "sound_pressure",
    ]

    return all(payload.get(key) is not None for key in required_keys)


# ---------------------------------------------------------------------------
# MQTT callbacks
# ---------------------------------------------------------------------------

def on_nas_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to NAS MQTT broker at {NAS_MQTT_HOST}:{NAS_MQTT_PORT}", flush=True)
    else:
        print(f"NAS MQTT connection failed. Result code: {rc}", flush=True)


def on_nas_disconnect(client, userdata, rc):
    if rc != 0:
        print("NAS MQTT disconnected unexpectedly.", flush=True)


def on_ha_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to HA MQTT broker at {HA_MQTT_HOST}:{HA_MQTT_PORT}", flush=True)

        if PUBLISH_HA_PAYLOAD:
            publish_ha_discovery(client)

            client.publish(
                HA_AVAILABILITY_TOPIC,
                payload="online",
                qos=MQTT_QOS,
                retain=RETAIN_AVAILABILITY,
            )

            print(f"Published HA availability: {HA_AVAILABILITY_TOPIC}", flush=True)
    else:
        print(f"HA MQTT connection failed. Result code: {rc}", flush=True)


def on_ha_disconnect(client, userdata, rc):
    if rc != 0:
        print("HA MQTT disconnected unexpectedly.", flush=True)


def connect_mqtt_client(client, host, port, label):
    """
    Connect one MQTT client.
    If connection fails, log it and continue.
    """

    try:
        client.connect(host, port, keepalive=60)
        client.loop_start()
        return True
    except Exception as exc:
        print(f"Could not connect to {label} MQTT broker at {host}:{port}: {exc}", flush=True)
        return False


def safe_publish(client, topic, payload, label, retain=False):
    """
    Publish without letting one failed broker kill the whole script.
    """

    try:
        result = client.publish(
            topic,
            payload=payload,
            qos=MQTT_QOS,
            retain=retain,
        )

        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            print(f"{label} publish failed for topic {topic}. MQTT result code: {result.rc}", flush=True)
            return False

        return True

    except Exception as exc:
        print(f"{label} publish error for topic {topic}: {exc}", flush=True)
        return False


# ---------------------------------------------------------------------------
# Home Assistant MQTT Discovery
# ---------------------------------------------------------------------------

def publish_ha_discovery(client):
    device = {
        "identifiers": [DEVICE_ID],
        "name": DEVICE_NAME,
        "manufacturer": "Raspberry Pi / MS430",
        "model": "MS430 Environmental Sensor",
        "suggested_area": LOCATION_ROOM.title(),
    }

    origin = {
        "name": "mqtt-home-data",
        "sw": "1.0",
        "url": "https://github.com/raspberrycoulis/mqtt-home-data",
    }

    sensors = [
        {
            "key": "temperature",
            "name": "Temperature",
            "device_class": "temperature",
            "state_class": "measurement",
            "unit": "°C",
            "icon": None,
            "precision": 1,
        },
        {
            "key": "humidity",
            "name": "Humidity",
            "device_class": "humidity",
            "state_class": "measurement",
            "unit": "%",
            "icon": None,
            "precision": 1,
        },
        {
            "key": "pressure",
            "name": "Pressure",
            "device_class": "pressure",
            "state_class": "measurement",
            "unit": "hPa",
            "icon": None,
            "precision": 1,
        },
        {
            "key": "illuminance",
            "name": "Illuminance",
            "device_class": "illuminance",
            "state_class": "measurement",
            "unit": "lx",
            "icon": None,
            "precision": 0,
        },
        {
            "key": "air_quality_index",
            "name": "Air Quality Index",
            "device_class": None,
            "state_class": "measurement",
            "unit": None,
            "icon": "mdi:air-filter",
            "precision": 0,
        },
        {
            "key": "air_quality_accuracy",
            "name": "Air Quality Accuracy",
            "device_class": None,
            "state_class": "measurement",
            "unit": None,
            "icon": "mdi:check-decagram-outline",
            "precision": 0,
        },
        {
            "key": "breath_voc",
            "name": "Breath VOC",
            "device_class": "volatile_organic_compounds_parts",
            "state_class": "measurement",
            "unit": "ppm",
            "icon": None,
            "precision": 2,
        },
        {
            "key": "estimated_co2",
            "name": "Estimated CO2",
            "device_class": "carbon_dioxide",
            "state_class": "measurement",
            "unit": "ppm",
            "icon": None,
            "precision": 0,
        },
        {
            "key": "gas_resistance",
            "name": "Gas Resistance",
            "device_class": None,
            "state_class": "measurement",
            "unit": "Ω",
            "icon": "mdi:resistor",
            "precision": 0,
        },
        {
            "key": "peak_amplitude",
            "name": "Peak Amplitude",
            "device_class": None,
            "state_class": "measurement",
            "unit": "mPa",
            "icon": "mdi:waveform",
            "precision": 2,
        },
        {
            "key": "sound_pressure",
            "name": "Sound Pressure",
            "device_class": "sound_pressure",
            "state_class": "measurement",
            "unit": "dB",
            "icon": None,
            "precision": 1,
        },
    ]

    for sensor_def in sensors:
        object_id = f"{DEVICE_ID}_{sensor_def['key']}"
        unique_id = object_id
        config_topic = f"{DISCOVERY_PREFIX}/sensor/{object_id}/config"

        payload = {
            "name": sensor_def["name"],
            "unique_id": unique_id,
            "object_id": object_id,
            "state_topic": HA_STATE_TOPIC,
            "availability_topic": HA_AVAILABILITY_TOPIC,
            "payload_available": "online",
            "payload_not_available": "offline",
            "value_template": "{{ value_json['" + sensor_def["key"] + "'] }}",
            "device": device,
            "origin": origin,
            "expire_after": EXPIRE_AFTER,
            "qos": MQTT_QOS,
            "suggested_display_precision": sensor_def["precision"],
        }

        if sensor_def["device_class"]:
            payload["device_class"] = sensor_def["device_class"]

        if sensor_def["state_class"]:
            payload["state_class"] = sensor_def["state_class"]

        if sensor_def["unit"]:
            payload["unit_of_measurement"] = sensor_def["unit"]

        if sensor_def["icon"]:
            payload["icon"] = sensor_def["icon"]

        safe_publish(
            client=client,
            topic=config_topic,
            payload=json.dumps(payload),
            label="HA discovery",
            retain=RETAIN_DISCOVERY,
        )

        print(f"Published HA discovery config: {config_topic}", flush=True)


# ---------------------------------------------------------------------------
# Shutdown handling
# ---------------------------------------------------------------------------

def cleanup_and_exit(nas_client=None, ha_client=None, gpio=None):
    print("Stopping.", flush=True)

    if ha_client and PUBLISH_HA_PAYLOAD:
        print("Publishing HA offline status.", flush=True)

        safe_publish(
            client=ha_client,
            topic=HA_AVAILABILITY_TOPIC,
            payload="offline",
            label="HA availability",
            retain=RETAIN_AVAILABILITY,
        )

        time.sleep(0.5)

    for client in [nas_client, ha_client]:
        if client:
            try:
                client.loop_stop()
                client.disconnect()
            except Exception:
                pass

    try:
        if gpio:
            gpio.cleanup()
    except Exception:
        pass

    sys.exit(0)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("Setting up MS430 hardware...", flush=True)

    gpio, i2c_bus = SensorHardwareSetup()

    i2c_bus.write_i2c_block_data(
        i2c_7bit_address,
        PARTICLE_SENSOR_SELECT_REG,
        [PARTICLE_SENSOR],
    )

    i2c_bus.write_i2c_block_data(
        i2c_7bit_address,
        CYCLE_TIME_PERIOD_REG,
        [CYCLE_PERIOD],
    )

    print("Entering cycle mode. Press Ctrl+C to exit.", flush=True)
    i2c_bus.write_byte(i2c_7bit_address, CYCLE_MODE_CMD)

    nas_client = None
    ha_client = None

    if PUBLISH_LEGACY_PAYLOAD:
        nas_client = mqtt.Client(client_id=f"{DEVICE_ID}_nas")
        nas_client.on_connect = on_nas_connect
        nas_client.on_disconnect = on_nas_disconnect

        if NAS_MQTT_USERNAME and NAS_MQTT_PASSWORD:
            nas_client.username_pw_set(NAS_MQTT_USERNAME, NAS_MQTT_PASSWORD)

        connect_mqtt_client(
            client=nas_client,
            host=NAS_MQTT_HOST,
            port=NAS_MQTT_PORT,
            label="NAS",
        )

    if PUBLISH_HA_PAYLOAD:
        ha_client = mqtt.Client(client_id=f"{DEVICE_ID}_ha")
        ha_client.on_connect = on_ha_connect
        ha_client.on_disconnect = on_ha_disconnect

        ha_client.username_pw_set(HA_MQTT_USERNAME, HA_MQTT_PASSWORD)

        ha_client.will_set(
            HA_AVAILABILITY_TOPIC,
            payload="offline",
            qos=MQTT_QOS,
            retain=RETAIN_AVAILABILITY,
        )

        connect_mqtt_client(
            client=ha_client,
            host=HA_MQTT_HOST,
            port=HA_MQTT_PORT,
            label="HA",
        )

    signal.signal(
        signal.SIGTERM,
        lambda signum, frame: cleanup_and_exit(nas_client, ha_client, gpio),
    )

    signal.signal(
        signal.SIGINT,
        lambda signum, frame: cleanup_and_exit(nas_client, ha_client, gpio),
    )

    while True:
        try:
            while not gpio.event_detected(READY_pin):
                time.sleep(0.05)

            legacy_payload, ha_payload = build_payloads(i2c_bus)

            if not payload_has_required_values(ha_payload):
                print(f"Skipping publish because payload has missing values: {ha_payload}", flush=True)
                time.sleep(PUBLISH_PERIOD)
                continue

            if PUBLISH_LEGACY_PAYLOAD and nas_client:
                legacy_ok = safe_publish(
                    client=nas_client,
                    topic=LEGACY_MQTT_TOPIC,
                    payload=json.dumps(legacy_payload),
                    label="NAS legacy",
                    retain=RETAIN_LEGACY_STATE,
                )

                if legacy_ok:
                    print(f"Published legacy payload to NAS topic: {LEGACY_MQTT_TOPIC}", flush=True)

            if PUBLISH_HA_PAYLOAD and ha_client:
                ha_ok = safe_publish(
                    client=ha_client,
                    topic=HA_STATE_TOPIC,
                    payload=json.dumps(ha_payload),
                    label="HA state",
                    retain=RETAIN_HA_STATE,
                )

                if ha_ok:
                    print(f"Published HA payload to topic: {HA_STATE_TOPIC}", flush=True)

            print(f"Publish cycle complete at {now_display()}", flush=True)

            time.sleep(PUBLISH_PERIOD)

        except KeyboardInterrupt:
            cleanup_and_exit(nas_client, ha_client, gpio)

        except Exception as exc:
            print(f"Error during sensor read/publish cycle: {exc}", flush=True)
            time.sleep(PUBLISH_PERIOD)


if __name__ == "__main__":
    main()
