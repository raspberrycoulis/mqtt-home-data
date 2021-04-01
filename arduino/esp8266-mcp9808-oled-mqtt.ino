#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include "Adafruit_MCP9808.h" // MCP9808 temperature sensor
#include <PubSubClient.h>

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, D2, D1);  //D2=SDK  D1=SCK  As per labeling on NodeMCU

// Initialise the MCP9808 sensor
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

// WiFi credentials
const char* ssid = "ADD_HERE";
const char* password = "ADD_HERE";

// Static IP address
IPAddress ip(192, 168, 1, 42); // Uses commas, not points!
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress primary_dns(192, 168, 1, 254);
IPAddress secondary_dns(1, 1, 1, 1);

// MQTT credentials
const char* mqtt_server = "192.168.1.24"; // Okay to use points now...
const char* channel = "sensors";
const char* level = "downstairs";
const char* room = "living-room";
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  delay(1000);
  Serial.begin(115200);
  WiFi.config(ip, gateway, subnet, primary_dns, secondary_dns);
  WiFi.begin(ssid, password);
  while (!Serial); // waits for serial terminal to be open, necessary in newer Arduino boards.
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected with IP ");
  Serial.println(WiFi.localIP());
  client.setServer(mqtt_server, 1883);
  Serial.println("Connected to MQTT server: " + String(mqtt_server));
  Serial.println("");
  Serial.println("Initialising MCP9808 sensor...");
  Serial.println("Initializing OLED display...");
  display.init();
  display.flipScreenVertically();
  
  if (!tempsensor.begin(0x18)) {
    Serial.println("Couldn't find the MCP9808 sensor! Check the connections and that the I2C address is correct.");
    while (1);
  }

  Serial.println("Found MCP9808!");
  tempsensor.setResolution(3); // sets the resolution of the reading:
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms
}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
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
  float c = tempsensor.readTempC() - 4.6; // Calibrated value for sensor if housed in a case
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
  Serial.println("Data sent to MQTT server...");
  delay(60000);
}
//=========================================================================
