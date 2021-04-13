#include <FS.h>
#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <Adafruit_TSL2561_U.h> // TSL2561 sensor library
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

// Initialise the TSL2561 sensor
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

// Static IP address
char static_ip[16] = "192.168.1.148";
char static_gateway[16] = "192.168.1.254";
char static_subnet[16] = "255.255.255.0";

// MQTT credentials
char mqtt_server[40] = "192.168.1.24";
char client_id[30] = "dev-room";
char channel[10] = "homedev";
char level[20] = "upstairs";
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

// Configure the TSL2651 sensor
void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("402 ms");
  Serial.println("------------------------------------");
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
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_client_id);
  wm.addParameter(&custom_channel);
  wm.addParameter(&custom_level);
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
  // Save the custom parameters to file system
  if (shouldSaveConfig) {
    Serial.println("Saving config...");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["client_id"] = client_id;
    json["channel"] = channel;
    json["level"] = level;
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
  Serial.println("Initialising TSL2561 sensor...");
  /* Initialise the sensor */
  //use tsl.begin() to default to Wire, 
  //tsl.begin(&Wire2) directs api to use Wire2, etc.
  if(!tsl.begin())
  {
    Serial.print("Ooops, no TSL2561 detected... Check your wiring or I2C ADDR!");
    while(1);
  }
  Serial.println("Found TSL2561 sensor!");
  configureSensor();
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
  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();

  // Get sensor data
  sensors_event_t event;
  tsl.getEvent(&event);

  // Construct JSON object with sensor data
  JSONencoder["device"] = String(client_id);
  JSONencoder["sensor"] = "TSL2561";
  JSONencoder["level"] = String(level);
  JSONencoder["lux"] = event.light;
 
  char JSONmessageBuffer[100];
  JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  Serial.println("Sending message to MQTT topic..");
  Serial.println(JSONmessageBuffer);
  client.publish(channel, JSONmessageBuffer, true);
  Serial.println("Data sent to MQTT server!");
  delay(60000);
}
