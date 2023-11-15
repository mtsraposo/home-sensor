#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ntp_connect.h>
#include <mqtt_secrets.h>

const int MQTT_CONNECTION_DELAY= 5000;
const int MAX_MQTT_RETRIES = 30;
const int MQTT_PORT = 8883; // MQTT = 8883, HTTPS = 8443, HTTP = 443
const long PUBLISH_DEBOUNCE_TIME = 5000;
unsigned long lastPublishTime = 0;

void setupMqtt(PubSubClient* mqttClient) {
  NTPConnect();

  mqttClient->setServer(mqttServer, MQTT_PORT);
}

int connectMqtt(PubSubClient* mqttClient) {
  int retries = 0;
  Serial.print("Connecting to MQTT server at ");
  Serial.println(mqttServer);
  while (!mqttClient->connected() && retries < MAX_MQTT_RETRIES) {
    if (mqttClient->connect(MQTT_CLIENT_ID)) {
      Serial.println();
      Serial.println("Connected.");
      return 0;
    } else {
      Serial.print(".");
      delay(MQTT_CONNECTION_DELAY);
      retries++;
    }
  }

  Serial.println();
  Serial.println("Max MQTT connection retries reached. Aborting...");
  
  return 1;
}

void publishToMqtt(PubSubClient* mqttClient) {
  if (millis() - lastPublishTime > PUBLISH_DEBOUNCE_TIME) {
      lastPublishTime = millis();
      char timestampStr[20];
      sprintf(timestampStr, "%ld", now);
      String detectedAt = String(timestampStr);

      StaticJsonDocument<200> doc;
      doc["detectedAt"] = detectedAt;
      doc["message"] = "presence";
      char jsonBuffer[512];
      serializeJson(doc, jsonBuffer);

      mqttClient->publish(MQTT_TOPIC, jsonBuffer);
  }
}

