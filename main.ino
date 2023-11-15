#include <wifi_secrets.h>
#include <wifi_utils.h>
#include <mqtt_client.h>

const int ledPin = 2;
const int inputPin = 5;
int pirState = LOW;
int pirVal;

std::unique_ptr<BearSSL::WiFiClientSecure> wifiClient(new BearSSL::WiFiClientSecure());
PubSubClient mqttClient(*wifiClient);

BearSSL::X509List cert(cacert);
BearSSL::X509List clientCert(client_cert);
BearSSL::PrivateKey privateKey(privkey);

void setup() {
    Serial.begin(9600);

    pinMode(ledPin, OUTPUT);
    pinMode(inputPin, INPUT);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting");
    awaitWifiConnection();

    wifiClient->setTrustAnchors(&cert);
    wifiClient->setClientRSACert(&clientCert, &privateKey);
    setupMqtt(&mqttClient);
    connectMqtt(&mqttClient);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi disconnected. Retrying connection..");
        awaitWifiConnection();
    }

    now = time(nullptr);

    if (!mqttClient.connected()) {
        Serial.println("MQTT disconnected. Attempting reconnection...");
        connectMqtt(&mqttClient);
    } else {
        mqttClient.loop();

        pirVal = digitalRead(inputPin);
        if (pirVal == HIGH) {
            digitalWrite(ledPin, HIGH);
            if (pirState == LOW) {
                publishToMqtt(&mqttClient);
                pirState = HIGH;
            }
        } else {
            digitalWrite(ledPin, LOW);
            if (pirState == HIGH){
                pirState = LOW;
            }
        }
    }
}