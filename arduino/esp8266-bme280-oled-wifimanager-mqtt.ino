#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <Adafruit_BME280.h> // BME280 sensor
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
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
  Serial.println("Initializing OLED display...");
  display.init();
  display.flipScreenVertically();
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

void displayTemperature() {
  //float temperature = bme.readTemperature() - 2.193; // Calibrated value for sensor if housed in a case
  float temperature = bme.readTemperature();
  Serial.print("Temperature: ");
  Serial.print(temperature, 4); Serial.println("°C.");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Temperature:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(temperature, 2) + "°C");
  display.display();
}

void displayHumidity() {
  float humidity = bme.readHumidity();
  Serial.print("Humidity: ");
  Serial.print(humidity, 2); Serial.println("% RH.");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Humidity:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(humidity, 2) + "% RH");
  display.display();
}

void displayPressure() {
  float pressure = bme.readPressure() / 100.0F;
  Serial.print("Pressure: ");
  Serial.print(pressure, 0); Serial.println("hPa.");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Pressure:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(pressure, 2) + "hPa");
  display.display();
}

void publishData() {
  //float temperature = bme.readTemperature() - 2.193; // Calibrated value for sensor if housed in a case
  float temperature = bme.readTemperature();
  String v1 = ("temperature,room=" + String(room) + ",floor=" + String(level) + " value=" + String(temperature));
  float humidity = bme.readHumidity();
  String v2 = ("humidity,room=" + String(room) + ",floor=" + String(level) + " value=" + String(humidity));
  float pressure = bme.readPressure() / 100.0F;
  String v3 = ("pressure,room=" + String(room) + ",floor=" + String(level) + " value=" + String(pressure));
  client.publish(channel, v1.c_str(), true);
  client.publish(channel, v2.c_str(), true);
  client.publish(channel, v3.c_str(), true);
  Serial.println("Temperature, humidity and pressure data sent to MQTT server!");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  publishData();
  delay(1000);
  displayTemperature();
  delay(20000);
  displayHumidity();
  delay(20000);
  displayPressure();
  delay(19000);
}