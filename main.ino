#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <WiFiClientSecureBearSSL.h>
#define TIME_ZONE -3

const int ledPin = 2;
const int inputPin = 5;

const int MQTT_CONNECTION_DELAY= 5000;
const int MQTT_PORT = 8883; // MQTT = 8883, HTTPS = 8443, HTTP = 443
const char* MQTT_CLIENT_CERTIFICATE_PATH = "/root-CA.crt.der";
const char* MQTT_CERTIFICATE_PATH = "/home_sensor.cert.der";
const char* MQTT_PRIVATE_KEY_PATH = "/home_sensor.private.der";
const char* MQTT_TOPIC = "sdk/test/js";
const int MAX_MQTT_RETRIES = 30;

const char* MQTT_CLIENT_ID = "basicPubSub";

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

time_t now;
time_t nowish = 1510592825;

void NTPConnect(void)
{
    Serial.print("Setting time using SNTP");
    configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
    now = time(nullptr);
    while (now < nowish)
    {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("done!");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(asctime(&timeinfo));
}

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

int authMqtt() {
    // Root certificate
    File certFile = SPIFFS.open(MQTT_CERTIFICATE_PATH, "r");
    if (!certFile) {
        Serial.println("Failed to open certificate file");
        return 1;
    }
    size_t certSize = certFile.size();
    std::unique_ptr<uint8_t[]> certBuf(new uint8_t[certSize]);
    certFile.readBytes(reinterpret_cast<char*>(certBuf.get()), certSize);
    BearSSL::X509List cert(certBuf.get(), certSize);
    certFile.close();

    // Client certificate
    File clientCertFile = SPIFFS.open(MQTT_CLIENT_CERTIFICATE_PATH, "r");
    if (!clientCertFile) {
        Serial.println("Failed to open client certificate file");
        return 1;
    }
    size_t clientCertSize = clientCertFile.size();
    std::unique_ptr<uint8_t[]> clientCertBuf(new uint8_t[clientCertSize]);
    clientCertFile.readBytes(reinterpret_cast<char*>(clientCertBuf.get()), clientCertSize);
    BearSSL::X509List clientCert(clientCertBuf.get(), clientCertSize);
    clientCertFile.close();

    // Private key
    File privateKeyFile = SPIFFS.open(MQTT_PRIVATE_KEY_PATH, "r");
    if (!privateKeyFile) {
        Serial.println("Failed to open private key file");
        return 1;
    }
    size_t privateKeySize = privateKeyFile.size();
    std::unique_ptr<uint8_t[]> privateKeyBuf(new uint8_t[privateKeySize]);
    privateKeyFile.readBytes(reinterpret_cast<char*>(privateKeyBuf.get()), privateKeySize);
    BearSSL::PrivateKey privateKey(privateKeyBuf.get(), privateKeySize);
    privateKeyFile.close();

    NTPConnect();

    Serial.printf("CACert size: %d\n", certSize);
    Serial.printf("Client cert size: %d\n", clientCertSize);
    Serial.printf("Private key size: %d\n", privateKeySize);
    wifiClientSecure->setTrustAnchors(&cert);
    wifiClientSecure->setClientRSACert(&clientCert, &privateKey);

    return 0;
}

int setupMqtt() {
    if (!mqttServer) {
        Serial.println("MQTT server not configured");
        return 1;
    }

    if (authMqtt()) {
        Serial.println("Failed to load MQTT certificate or private key");
        return 1;
    }

    client.setServer(mqttServer.c_str(), MQTT_PORT);
    return 0;
}

int connectMqtt() {
    int retries = 0;
    Serial.print("Connecting to MQTT server at ");
    Serial.println(mqttServer);
    while (!client.connected() && retries < MAX_MQTT_RETRIES) {
        if (client.connect(MQTT_CLIENT_ID)) {
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
    mqttServer = (*config)["mqttServer"].as<String>();;
    Serial.print("MQTT Server: ");
    Serial.println(mqttServer);
    delete config;

    pinMode(ledPin, OUTPUT);
    pinMode(inputPin, INPUT);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Connecting");
    awaitWifiConnection();

    setupMqtt();
    connectMqtt();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi disconnected. Retrying connection..");
        awaitWifiConnection();
    }

    now = time(nullptr);

    if (!client.connected()) {
        Serial.println("MQTT disconnected. Attempting reconnection...");
        connectMqtt();
    } else {
        Serial.println("MQTT connected. Looping...");
        client.loop();

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
}