#include <Arduino.h>
#include "DHT.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// Constants for sensor pin and type
#define DHT_PIN 21
#define DHT_TYPE DHT22

#define CLIENT_ID "demo"
#define INTERVAL_S 30 // Data sending interval in seconds

// WiFi router credentials
const char SSID[] = "<CHANGE THIS>";
const char PASSWD[] = "<CHANGE THIS>";

// MQTT broker details
const char BROKER[] = "192.168.x.y";
const uint16_t BROKER_PORT = 1883;

const char TEMP_TOPIC[] = "ws/" CLIENT_ID "/temp";
const char HUM_TOPIC[] = "ws/" CLIENT_ID "/hum";

// Sensor object
DHT dhtSensor(DHT_PIN, DHT_TYPE);

// Network and mqtt client
WiFiClient espClient;
PubSubClient mqttClient(espClient); // Documentation: https://pubsubclient.knolleary.net/api

// Function declaration
void printData(float t, float h);
bool connectWifi();
bool connectMqtt();
void sendData(float t, float h);
void mqttLoop();

// Variable to store time for data sending purposes
uint32_t prevTime;

void setup()
{
  // WiFi station mode - will act as client and not as access point
  WiFi.mode(WIFI_STA);
  // Prevent storing WiFi credentials in flash memory
  WiFi.persistent(false);
  Serial.begin(115200);

  mqttClient.setServer(BROKER, BROKER_PORT);
  dhtSensor.begin();
  prevTime = millis();
}

void loop()
{
  // Send data every INTERVAL_S
  if (millis() - prevTime > INTERVAL_S * 1000)
  {
    float temperature = dhtSensor.readTemperature();
    float humidity = dhtSensor.readHumidity();
    // If one of the readings is not a number, discard the data
    if (!(isnan(temperature) || isnan(humidity)))
    {
      printData(temperature, humidity);
      if (connectWifi() && connectMqtt())
      {
        Serial.println("Sending data");
        sendData(temperature, humidity);
        Serial.println("Done");
      }
    }
    else
    {
      Serial.println("Sensor error");
    }
    // Update time variable
    prevTime = millis();
  }
}

void printData(float t, float h)
{
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" humidity: ");
  Serial.println(h);
}

bool connectWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi already connected");
    return true;
  }

  Serial.print("Connecting");
  // A counter to exit loop if the connection failed after several attempts
  int counter = 0;
  WiFi.begin(SSID, PASSWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    // Avoid infinite loop
    if (counter > 50)
    {
      Serial.println("Couldn't connect to network");
      return false;
    }
    counter++;
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
  return true;
}

bool connectMqtt()
{
  Serial.print("Connecting to MQTT broker");
  if (mqttClient.connected())
  {
    Serial.println(" - already connected");
    return true;
  }

  // A counter to exit loop if the connection failed after several attempts
  uint8_t loopCounter = 0;
  while (!mqttClient.connect(CLIENT_ID))
  {
    Serial.print(".");

    espClient.flush();
    espClient.stop();
    // Avoid infinite loop
    if (loopCounter > 2)
    {
      Serial.println(" - not connected");
      return false;
    }
    loopCounter++;
    delay(1000);
  }
  Serial.println(" - connected");
  return true;
}

void sendData(float t, float h)
{
  // Buffer to store temperature as a char array for better readability at the subscriber end
  char tempBuffer[7]; // 6 characters (4 digits, decimal separator, sign) + null character '\0' to terminate the array
  snprintf(tempBuffer, sizeof(tempBuffer), "%f", t);
  // Same for humidity, but 5 characters (4 digits, decimal separator) + null character '\0'
  char humBuffer[6];
  snprintf(humBuffer, sizeof(humBuffer), "%f", h);
  // Loop a couple of times to be sure the connection is established and "keep alive" message is sent
  mqttLoop();
  mqttClient.publish(TEMP_TOPIC, tempBuffer);
  mqttClient.publish(HUM_TOPIC, humBuffer);
  // Loop a couple of times so the client does not disconnect before data is sent (improves data sending)
  mqttLoop();
  mqttClient.disconnect();
  mqttLoop();
}

void mqttLoop()
{
  for (int8_t i = 0; i < 10; i++)
  {
    mqttClient.loop();
    delay(10);
  }
}
