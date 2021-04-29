#include <FS.h>
#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
//#include "Adafruit_Si7021.h"// Si7201 temperature sensor
#include "Adafruit_HTU21DF.h" // Some cheap Si7201 sensors are HTU21D-F sensors, so use this if so.
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <ArduinoJson.h>
Ticker ticker;

#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // ESP32 DOES NOT DEFINE LED_BUILTIN
#endif

#ifdef ESP32
  #include <SPIFFS.h>
#endif

// For LED ticker
int LED = LED_BUILTIN;

// Initialise the Si7201 sensor
//Adafruit_Si7021 sensor = Adafruit_Si7021(); // Use for Adafruit Si7021 sensors
Adafruit_HTU21DF htu = Adafruit_HTU21DF(); // Use if sensor is not found with Si7021 library

// Static IP address
char static_ip[16] = "192.168.1.148";
char static_gateway[16] = "192.168.1.254";
char static_subnet[16] = "255.255.255.0";

// MQTT credentials
char mqtt_server[40] = "192.168.1.24";
char client_id[30] = "dev-room";
char channel[10] = "homedev";
char level[20] = "upstairs";
char temp_calibration[6] = "6.5";
WiFiClient espClient;
PubSubClient client(espClient);

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void tick()
{
  //toggle state
  digitalWrite(LED, !digitalRead(LED));     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

void setupSpiffs(){
  //read configuration from FS json
  Serial.println("");
  Serial.println("Mounting File system...");

  if (SPIFFS.begin()) {
    Serial.println("Mounted file system!");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("Reading config file...");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Opened config file!");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nParsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(client_id, json["client_id"]);
          strcpy(channel, json["channel"]);
          strcpy(level, json["level"]);
          strcpy(temp_calibration, json["temp_calibration"]);

          if(json["ip"]) {
            Serial.println("Setting custom IP from config");
            strcpy(static_ip, json["ip"]);
            strcpy(static_gateway, json["gateway"]);
            strcpy(static_subnet, json["subnet"]);
            Serial.println(static_ip);
          } else {
            Serial.println("No custom IP address in config");
          }

        } else {
          Serial.println("Failed to load json config!");
        }
      }
    }
  } else {
    Serial.println("Failed to mount file system!");
  }
}

void setup() {
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.begin(115200);
  pinMode(LED, OUTPUT); // Set LED pin as output
  ticker.attach(0.6, tick);
  setupSpiffs();
  //WiFiManager
  WiFiManager wm;
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT server", mqtt_server, 40);
  WiFiManagerParameter custom_client_id("client_id", "Name of this client", client_id, 30);
  WiFiManagerParameter custom_channel("channel", "MQTT channel", channel, 10);
  WiFiManagerParameter custom_level("level", "Floor in house", level, 20);
  WiFiManagerParameter custom_temp_calibration("temp_calibration", "Temperature calibration", temp_calibration, 6);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_client_id);
  wm.addParameter(&custom_channel);
  wm.addParameter(&custom_level);
  wm.addParameter(&custom_temp_calibration);
  // Set static IP address
  IPAddress _ip,_gateway,_subnet;
  _ip.fromString(static_ip);
  _gateway.fromString(static_gateway);
  _subnet.fromString(static_subnet);
  wm.setSTAStaticIPConfig(_ip, _gateway, _subnet);
  wm.setAPCallback(configModeCallback);
  if (!wm.autoConnect()) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
    delay(1000);
  }
  Serial.println("");
  Serial.print("WiFi connected with IP ");
  Serial.println(WiFi.localIP());
  ticker.detach();
  digitalWrite(LED, HIGH);  //LOW = LED on, HIGH = LED off
  // Read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(client_id, custom_client_id.getValue());
  strcpy(channel, custom_channel.getValue());
  strcpy(level, custom_level.getValue());
  strcpy(temp_calibration, custom_temp_calibration.getValue());
  // Save the custom parameters to file system
  if (shouldSaveConfig) {
    Serial.println("Saving config...");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["client_id"] = client_id;
    json["channel"] = channel;
    json["level"] = level;
    json["temp_calibration"] = temp_calibration;
    json["ip"] = WiFi.localIP().toString();
    json["gateway"] = WiFi.gatewayIP().toString();
    json["subnet"] = WiFi.subnetMask().toString();
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing!");
    }
    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    Serial.println("Done saving!");
    //end save
    shouldSaveConfig = false;
  }
  client.setServer(mqtt_server, 1883);
  Serial.println("Connected to MQTT server: " + String(mqtt_server));
  Serial.println("");
  Serial.println("Initialising Si7201 sensor...");
  /* Initialise the Si7201 sensor */
  //if (!sensor.begin()) { // Use for Adafruit Si7021 sensors
  if (!htu.begin()) { // Use for cheap "Si7021" sensors
    Serial.println("Couldn't find the Si7201 sensor! Check the connections and that the I2C address is correct.");
    while (1);
  }
  Serial.println("Found Si7201 sensor!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(client_id)) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  // For temperature calibration
  char* tc = temp_calibration;
  char temp_calib_alt = (char)atoi(tc);
  float temp_calib = (float)temp_calib_alt;
  // Get sensor data from Si7201
  //float temperature = sensor.readTemperature() - temp_calib; // Use for Adafruit Si7021 sensors
  float temperature = htu.readTemperature() - temp_calib; // Use for cheap "Si7021" sensors
  //float humidity = sensor.readHumidity(); // Use for Adafruit Si7021 sensors
  float humidity = htu.readHumidity(); // Use for cheap "Si7021" sensors
  
  // Do JSON stuff
  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();

  // Start of JSON configuration options - use only one set!
  // Use below if EasyMQTT iOS app is not needed
  //JSONencoder["device"] = String(client_id);
  //JSONencoder["sensor"] = "Si7201";
  //JSONencoder["level"] = String(level);
  //JSONencoder["temperature"] = temperature;
  //JSONencoder["humidity"] = humidity;

  // Use below if EasyMQTT iOS app IS needed
  JsonObject& JSONencoderNested = JSONencoder.createNestedObject(client_id);
  JSONencoderNested["temperature"] = temperature;
  JSONencoderNested["humidity"] = humidity;
  JSONencoder["device"] = String(client_id);
  JSONencoder["sensor"] = "Si7201";
  JSONencoder["level"] = String(level);
  // End of JSON coniguration options - be sure to comment out one set!
  
  // Contiune to do JSON stuff
  char JSONmessageBuffer[200];
  JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));

  // Send data to MQTT broker
  Serial.println("Sending message to MQTT topic..");
  Serial.println(JSONmessageBuffer);
  client.publish(channel, JSONmessageBuffer, true);
  Serial.println("Temperature and humidity data sent to MQTT server!");
  delay(60000);
}
