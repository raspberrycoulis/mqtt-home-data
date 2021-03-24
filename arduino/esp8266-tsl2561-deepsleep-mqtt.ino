#include <ESP8266WiFi.h>  // To connect to WiFi
#include <Adafruit_TSL2561_U.h> // TSL2561 sensor library
#include <PubSubClient.h>  // MQTT client library

long SLEEPTIME = 600e6; // 10min
const char* ssid = "ADD_HERE";
const char* password = "ADD_HERE";

// Initialise the TSL2561 sensor
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

// We make a structure to store connection information
// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.
struct {
  uint32_t crc32;   // 4 bytes
  uint8_t channel;  // 1 byte,   5 in total
  uint8_t ap_mac[6];// 6 bytes, 11 in total
  uint8_t padding;  // 1 byte,  12 in total
} rtcData;

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
const char* room = "ADD_HERE";
WiFiClient espClient;
PubSubClient client(espClient);

// For battery voltage reporting
unsigned int batt;
ADC_MODE(ADC_VCC);


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
  // We disable WiFi, coming from DeepSleep, as we do not need it right away
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
  Serial.println("wifi uit");

  // Try to read WiFi settings from RTC memory
  bool rtcValid = false;
  if (ESP.rtcUserMemoryRead(0, (uint32_t*)&rtcData, sizeof(rtcData))) {
    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32(((uint8_t*)&rtcData) + 4, sizeof(rtcData) - 4);
    if (crc == rtcData.crc32) {
      rtcValid = true;
    }
  }
  // Report the battery level in the serial console
  batt = ESP.getVcc();
  Serial.print("Battery Voltage ");
  Serial.println(batt/1023.0F);

  // Start connection WiFi
  //Switch Radio back On
  WiFi.forceSleepWake();
  delay(1);

  // Disable the WiFi persistence.  The ESP8266 will not load and save WiFi settings unnecessarily in the flash memory.
  WiFi.persistent(false);
 Serial.println("Start WiFi");
  // Bring up the WiFi connection
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, primary_dns, secondary_dns);
  //-----------Now we replace the normally used "WiFi.begin();" with a precedure using connection data stored by us
  if (rtcValid) {
    // The RTC data was good, make a quick connection
    WiFi.begin(ssid, password, rtcData.channel, rtcData.ap_mac, true);
  }
  else {
    // The RTC data was not valid, so make a regular connection
    WiFi.begin(ssid, password);
  }

  //------now wait for connection
  int retries = 0;
  int wifiStatus = WiFi.status();
  while (wifiStatus != WL_CONNECTED) {
    retries++;
    if (retries == 100) {
      // Quick connect is not working, reset WiFi and try regular connection
      WiFi.disconnect();
      delay(10);
      WiFi.forceSleepBegin();
      delay(10);
      WiFi.forceSleepWake();
      delay(10);
      WiFi.begin(ssid, password);
    }
    if (retries == 600) {
      // Giving up after 30 seconds and going back to sleep
      WiFi.disconnect(true);
      delay(1);
      WiFi.mode(WIFI_OFF);
      ESP.deepSleep(SLEEPTIME, WAKE_RF_DISABLED);
      return; // Not expecting this to be called, the previous call will never return.
    }
    delay(50);
    wifiStatus = WiFi.status();
  }
  //---------
  Serial.println("");
  Serial.print("WiFi connected with IP ");
  Serial.println(WiFi.localIP());

  //-----
  // Write current connection info back to RTC
  rtcData.channel = WiFi.channel();
  memcpy(rtcData.ap_mac, WiFi.BSSID(), 6); // Copy 6 bytes of BSSID (AP's MAC address)
  rtcData.crc32 = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof(rtcData) - 4);
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));
  client.setServer(mqtt_server, 1883);
  Serial.println("Connected to MQTT server: " + String(mqtt_server));
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
  Serial.println("Start sending data...");
  sendMQTTmessage();
  Serial.println("Going back to sleep...");
  WiFi.disconnect(true);
  delay(1);
  // WAKE_RF_DISABLED to keep the WiFi radio disabled when we wake up
  ESP.deepSleep(SLEEPTIME, WAKE_RF_DISABLED);
}


void loop() {

}

void sendMQTTmessage()
{
  if (!client.connected()) {
    reconnect();
  }
  sensors_event_t event;  // TSL2561
  tsl.getEvent(&event);   // TSL2561
  Serial.print("Light: ");
  Serial.print(event.light, 4); Serial.print(" lux\t.");
  Serial.println("");
  String v1 = ("lux,room=" + String(room) + ",floor=" + String(level) + " value=" + String(event.light));
  String v2 = ("battery,room=" + String(room) + ",floor=" + String(level) + " value=" + String(batt / 1023.0F));
  client.publish(channel, v1.c_str(), true);
  client.publish(channel, v2.c_str(), true);  
  Serial.println("Data sent to MQTT server!");
  client.disconnect();
}

void reconnect() {
  while (!client.connected())
  {
    String ClientId = "ESP8266";
    ClientId += String(random(0xffff), HEX);
    if (client.connect(ClientId.c_str()))
    {
      Serial.println("Connected");
    } else {
      Serial.println("Failed, rc= ");
      Serial.println(client.state());
      delay(1000);
    }

  }
}


// the CRC routine
uint32_t calculateCRC32( const uint8_t *data, size_t length ) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }

      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }

  return crc;
}