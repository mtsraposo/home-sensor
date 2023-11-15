#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>

const int MAX_WIFI_RETRIES = 30;

void wifiRestart(){
  Serial.println();
  Serial.println("Turning WiFi off...");
  WiFi.mode(WIFI_OFF);
  Serial.println("Sleepping for 10 seconds...");
  delay(10000);
  Serial.println("Trying to connect to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

void awaitWifiConnection() {
  int wifiConnectionRetries = 0;

  while (WiFi.status() != WL_CONNECTED && wifiConnectionRetries < MAX_WIFI_RETRIES) {
    delay(500);
    Serial.print(".");
    wifiConnectionRetries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    wifiRestart();
    return;
  }

  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

