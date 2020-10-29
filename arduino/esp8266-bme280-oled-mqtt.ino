#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <PubSubClient.h>

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, D2, D1);  //D2=SDK  D1=SCK  As per labeling on NodeMCU

// Configure the BME280 sensor
Adafruit_BME280 bme; // I2C

// WiFi credentials
const char* ssid = "ADD_HERE";
const char* password = "ADD_HERE";
const char* mqtt_server = "192.168.1.24";
const char* channel = "sensors";
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  delay(1000);
  Serial.begin(115200);

  // initialize BME280 sensor
  bool status;
  status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  
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
  Serial.println("Initialising BME280 sensor...");
  Serial.println("Initializing OLED display...");
  display.init();
  display.flipScreenVertically();
  Serial.println("Found BME280!");
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
  //float temperature = bme.readTemperature() - 2.193; // Calibrated value for sensor if housed in a case
  float c = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0F;
  Serial.print("Temperature: ");
  Serial.print(c, 4); Serial.print("°C\t.");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Temperature:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(c, 2) + "°C");
  display.display();
  delay(1000);
  Serial.print("Humidity: ");
  Serial.print(h, 4); Serial.print("% RH\t.");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Humidity:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(h, 2) + "% RH");
  display.display();
  delay(1000);
  Serial.print("Pressure: ");
  Serial.print(p, 4); Serial.print("hPa\t.");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Pressure:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(p, 2) + "hPa");
  display.display();
  String v1 = ("temp,room=living-room value=" + String(c));
  String v2 = ("humidity,room=living-room value=" + String(h));
  String v3 = ("pressure,room=living-room value=" + String(p)); 
  client.publish(channel, v1.c_str(), true);
  client.publish(channel, v1.h_str(), true);
  client.publish(channel, v3.p_str(), true);
  Serial.println("Data sent to MQTT server...");
  delay(15000);
}
//=========================================================================
