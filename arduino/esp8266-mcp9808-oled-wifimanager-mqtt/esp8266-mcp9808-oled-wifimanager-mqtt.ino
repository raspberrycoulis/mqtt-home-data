#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include "Adafruit_MCP9808.h" // MCP9808 temperature sensor
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Ticker.h>
Ticker ticker;

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, D2, D1);  //D2=SDK  D1=SCK  As per labeling on NodeMCU

#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // ESP32 DOES NOT DEFINE LED_BUILTIN
#endif

// For LED ticker
int LED = LED_BUILTIN;

// Initialise the MCP9808 sensor
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

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
IPAddress ip(192, 168, 1, 42); // Uses commas, not points!
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress primary_dns(192, 168, 1, 254);

// MQTT credentials
const char* mqtt_server = "192.168.1.24"; // Okay to use points now...
const char* channel = "sensors";
const char* level = "downstairs";
const char* room = "living-room";
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
  Serial.println("Initializing OLED display...");
  display.init();
  display.flipScreenVertically();
  Serial.println("Initialising MCP9808 sensor...");
  /* Initialise the sensor */
  if (!tempsensor.begin(0x18)) {
    Serial.println("Couldn't find the MCP9808 sensor! Check the connections and that the I2C address is correct.");
    while (1);
  }
  Serial.println("Found MCP9808 sensor!");
  tempsensor.setResolution(3); // sets the resolution of the reading:
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms
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
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //float c = tempsensor.readTempC(); // Use if sensor is not in a case
  float c = tempsensor.readTempC() - 6.5; // Calibrated value for sensor if housed in a case
  Serial.print("Temp: ");
  Serial.print(c, 4); Serial.print("°C\t.");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Temperature:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(c, 2) + "°C");
  display.display();
  Serial.println("");
  String v1 = ("temperature,room=" + String(room) + ",floor=" + String(level) + " value=" + String(c));
  client.publish(channel, v1.c_str(), true);
  Serial.println("Data sent to MQTT server!");
  delay(60000);
}