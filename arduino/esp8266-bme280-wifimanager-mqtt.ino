#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <Adafruit_BME280.h> // BME280 sensor
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Ticker.h>
Ticker ticker;

#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // ESP32 DOES NOT DEFINE LED_BUILTIN
#endif

// For LED ticker
int LED = LED_BUILTIN;

// Configure the BME280 sensor
Adafruit_BME280 bme; // I2C

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

// Static IP address
IPAddress ip(192, 168, 1, 48); // Uses commas, not points!
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress primary_dns(192, 168, 1, 254);

// MQTT credentials
const char* mqtt_server = "192.168.1.24"; // Okay to use points now...
const char* channel = "sensors";
const char* level = "upstairs";
const char* room = "dev";
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.begin(115200);
  pinMode(LED, OUTPUT); // Set LED pin as output
  ticker.attach(0.6, tick);
  //WiFiManager
  WiFiManager wm;
  // Set static IP address
  wm.setSTAStaticIPConfig(IPAddress(ip), IPAddress(gateway), IPAddress(subnet), IPAddress(primary_dns));
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
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
  client.setServer(mqtt_server, 1883);
  Serial.println("Connected to MQTT server: " + String(mqtt_server));
  Serial.println("");
  Serial.println("Initialising BME280 sensor...");
  bool status;
  status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  Serial.println("Found BME280 sensor!");
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
  //float temperature = bme.readTemperature() - 2.193; // Calibrated value for sensor if housed in a case
  float temperature = bme.readTemperature();
  String v1 = ("temperature,room=" + String(room) + ",floor=" + String(level) + " value=" + String(temperature));
  float humidity = bme.readHumidity();
  String v2 = ("humidity,room=" + String(room) + ",floor=" + String(level) + " value=" + String(humidity));
  float pressure = bme.readPressure() / 100.0F;
  String v3 = ("pressure,room=" + String(room) + ",floor=" + String(level) + " value=" + String(pressure));
  Serial.print("Temperature: ");
  Serial.println(temperature, 4); Serial.print("°C\t.");
  Serial.print("Humidity: ");
  Serial.println(humidity, 2); Serial.print("% RH\t.");
  Serial.print("Pressure: ");
  Serial.println(temperature, 0); Serial.print("hPa\t.");
  client.publish(channel, v1.c_str(), true);
  client.publish(channel, v2.c_str(), true);
  client.publish(channel, v3.c_str(), true);
  Serial.print("Temperature, humidity and pressure data sent to MQTT server!");
  delay(60000);
}