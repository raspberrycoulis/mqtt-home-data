#include <ESP8266WiFi.h> // To connect to WiFi
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include <Adafruit_TSL2561_U.h> // TSL2561 sensor library
#include <PubSubClient.h>

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, D2, D1);  //D2=SDK  D1=SCK  As per labeling on NodeMCU

// Initialise the TSL2561 sensor
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

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
  Serial.println("Initialising TSL2561 sensor...");
  /* Initialise the sensor */
  //use tsl.begin() to default to Wire, 
  //tsl.begin(&Wire2) directs api to use Wire2, etc.
  if(!tsl.begin())
  {
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  Serial.println("Found TSL2561 sensor!");
  configureSensor();
  Serial.println("Initializing OLED display...");
  display.init();
  display.flipScreenVertically();
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
  sensors_event_t event;                          // TSL2561
  tsl.getEvent(&event);                           // TSL2561
  Serial.print("Light: ");
  Serial.print(event.light, 4); Serial.print(" lux\t.");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Light:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, String(event.light, 2) + " lux");
  display.display();
  Serial.println("");
  String v1 = ("lux,room=" + String(room) + ",floor=" + String(level) + " value=" + String(event.light));
  client.publish(channel, v1.c_str(), true);
  Serial.println("Data sent to MQTT server...");
  delay(15000);
}
//=========================================================================
