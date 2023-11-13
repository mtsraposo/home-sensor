#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecureBearSSL.h>

const int ledPin = 2;
const int inputPin = 5;

const int MQTT_CONNECTION_DELAY= 10000;
const int MQTT_PORT = 8883; // TLS
const char* MQTT_CERTIFICATE_PATH = "/home_sensor.cert.der";
const char* MQTT_PRIVATE_KEY_PATH = "/home_sensor.private.der";
const char* MQTT_TOPIC = "sensor/presence";
const int MAX_MQTT_RETRIES = 30;

const char* CONFIG_FILE_PATH = "/config.json";

const int MAX_WIFI_RETRIES = 30;
String ssid;
String password;
String mqttServer;

int pirState = LOW;
int val = 0;
int count = 0;
int requests = 0;

const long PUBLISH_DEBOUNCE_TIME = 5000;
unsigned long lastPublishTime = 0;

std::unique_ptr<BearSSL::WiFiClientSecure> wifiClientSecure(new BearSSL::WiFiClientSecure());
PubSubClient client(*wifiClientSecure);

DynamicJsonDocument* initializeConfig() {
    File configFile = SPIFFS.open(CONFIG_FILE_PATH, "r");
    if (!configFile) {
        Serial.println("Failed to open config file");
        return nullptr;
    }

    size_t size = configFile.size();
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    configFile.close();

    auto config = new DynamicJsonDocument(1024);
    DeserializationError error = deserializeJson(*config, buf.get());

    if (error) {
        Serial.println("Failed to parse config file");
        delete config;
        return nullptr;
    }

    return config;
}

void wifiRestart(){
    Serial.println();
    Serial.println("Turning WiFi off...");
    WiFi.mode(WIFI_OFF);
    Serial.println("Sleepping for 10 seconds...");
    delay(10000);
    Serial.println("Trying to connect to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
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

int loadCertificate() {
    File file = SPIFFS.open(MQTT_CERTIFICATE_PATH, "r");
    if (!file) {
        Serial.println("Failed to open certificate file");
        return 1;
    }

    size_t size = file.size();
    std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
    file.readBytes(reinterpret_cast<char*>(buf.get()), size);
    BearSSL::X509List cert(buf.get(), size);
    wifiClientSecure->setTrustAnchors(&cert);
    file.close();
    return 0;
}

int loadPrivateKey() {
    File file = SPIFFS.open(MQTT_PRIVATE_KEY_PATH, "r");
    if (!file) {
        Serial.println("Failed to open private key file");
        return 1;
    }

    size_t size = file.size();
    std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
    file.readBytes(reinterpret_cast<char*>(buf.get()), size);
    wifiClientSecure->setPrivateKey(buf.get(), size);
    file.close();
    return 0;
}

int connectMqtt() {
    if (!mqttServer) {
        Serial.println("MQTT server not configured");
        return 1;
    }

    if (loadCertificate() || loadPrivateKey()) {
        Serial.println("Failed to load MQTT certificate or private key");
        return 1;
    }

    int retries = 0;
    client.setServer(mqttServer.c_str(), MQTT_PORT);
    while (!client.connected() && retries < MAX_MQTT_RETRIES) {
        if (client.connect("ESP8266Client")) {
            Serial.println("Connected to MQTT server");
            return 0;
        } else {
            Serial.printf("Failed to connect to MQTT server. Retrying in %d seconds...", MQTT_CONNECTION_DELAY);
            Serial.println();
            delay(MQTT_CONNECTION_DELAY);
            retries++;
        }
    }

    Serial.println("Max MQTT connection retries reached. Aborting...");

    return 1;
}

void publishToMqtt() {
    if (millis() - lastPublishTime > PUBLISH_DEBOUNCE_TIME) {
        lastPublishTime = millis();
        String payload = "{\"message\": \"presence\", \"lastPublished\": ";
        payload += lastPublishTime;
        payload += "}";
        client.publish(MQTT_TOPIC, payload.c_str());
    }
}

void setup() {
    Serial.begin(9600);

    if (!SPIFFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
    }

    DynamicJsonDocument* config = initializeConfig();
    if (config == nullptr) {
        Serial.println("Config initialization failed");
        return;
    }

    ssid = (*config)["ssid"].as<String>();
    password = (*config)["password"].as<String>();
    mqttServer = (*config)["mqttServer"].as<String>();
    delete config;

    pinMode(ledPin, OUTPUT);
    pinMode(inputPin, INPUT);

    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Connecting");
    awaitWifiConnection();

    connectMqtt();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi disconnected. Retrying connection..");
        awaitWifiConnection();
    }

    if (!client.connected()) {
        Serial.println("MQTT disconnected. Attempting reconnection...");
        connectMqtt();
    }
    client.loop();

    if (requests == 0) {
        client.publish(MQTT_TOPIC, "{\"message\": \"initialized presence loop\"}");
        requests++;
    }

    val = digitalRead(inputPin);
    if (val == HIGH) {
        digitalWrite(ledPin, HIGH);
        if (pirState == LOW) {
            Serial.print("Motion detected: ");
            Serial.println(++count);
            publishToMqtt();
            pirState = HIGH;
        }
    } else {
        digitalWrite(ledPin, LOW);
        if (pirState == HIGH){
            Serial.print("Motion ended: ");
            Serial.println(++count);
            pirState = LOW;
        }
    }
}