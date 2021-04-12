#include <FS.h>
#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
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

// Static IP address
char static_ip[16] = "192.168.1.148";
char static_gateway[16] = "192.168.1.254";
char static_subnet[16] = "255.255.255.0";

// MQTT credentials
char mqtt_server[40] = "192.168.1.30";
char channel[10] = "sensors";
char level[10] = "upstairs";
char room[20] = "dev";
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
          strcpy(channel, json["channel"]);
          strcpy(level, json["level"]);
          strcpy(room, json["room"]);

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
  WiFiManagerParameter custom_channel("channel", "MQTT channel", channel, 10);
  WiFiManagerParameter custom_level("level", "Floor in house", level, 10);
  WiFiManagerParameter custom_room("room", "Room in house", room, 20);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_channel);
  wm.addParameter(&custom_level);
  wm.addParameter(&custom_room);
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
  strcpy(channel, custom_channel.getValue());
  strcpy(level, custom_level.getValue());
  strcpy(room, custom_room.getValue());
  // Save the custom parameters to file system
  if (shouldSaveConfig) {
    Serial.println("Saving config...");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["channel"] = channel;
    json["level"] = level;
    json["room"] = room;
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
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();
 
  JSONencoder["device"] = "ESP8266";
  JSONencoder["sensorType"] = "Test";
  JsonArray& values = JSONencoder.createNestedArray("values");
 
  values.add(999);
 
  char JSONmessageBuffer[100];
  JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  Serial.println("Sending message to MQTT topic..");
  Serial.println(JSONmessageBuffer);
  client.publish(channel, JSONmessageBuffer, true);
  //String v1 = ("test,room=" + String(room) + ",floor=" + String(level) + " value=" + String("999"));
  //Serial.println("Random bullshit: 999");
  //client.publish(channel, v1.c_str(), true);
  Serial.println("Data sent to MQTT server!");
  delay(2000);
}
