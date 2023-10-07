#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

int ledPin = 2;
int inputPin = 5;
int pirState = LOW;
int val = 0;
int count = 0;

const char* ssid = "x";
const char* password = "x";

int requests = 0;
const char* url = "https://camo.githubusercontent.com/bc3509f02e6bcae7fd460fb19d6fae09ec3714c1b80339b99844335086572c4c/68747470733a2f2f6b6f6d617265762e636f6d2f67687076632f3f757365726e616d653d6d74737261706f736f";
const uint8_t fingerprint[20] = {0xA1,0x46,0x14,0xC7,0x2A,0x1D,0x52,0x79,0xF6,0xAA,0x2B,0xB2,0xC5,0x0A,0x3B,0xD3,0xF5,0x02,0x06,0x75};

void https_get(const char* url) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Error in WiFi connection");
        return;
    }

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint);

    HTTPClient https;
    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, url)) {
        Serial.print("[HTTPS] GET...\n");
        int httpCode = https.GET();

        if (httpCode > 0) {
            Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                String payload = https.getString();
                Serial.println(payload);
            }
        } else {
            Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }

        https.end();
    } else {
        Serial.printf("[HTTPS] Unable to connect\n");
    }
}

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

void await_connection() {
    int wifiConnectionRetries = 0;

    while (WiFi.status() != WL_CONNECTED && wifiConnectionRetries < 30) {
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

void setup() {
    Serial.begin(9600);

    pinMode(ledPin, OUTPUT);
    pinMode(inputPin, INPUT);

    WiFi.begin(ssid, password);

    Serial.print("Connecting");
    await_connection();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi disconnected. Retrying connection..");
        await_connection();
    }

    if (requests == 0) {
        https_get(url);
        requests++;
    }

    val = digitalRead(inputPin);
    if (val == HIGH) {
        digitalWrite(ledPin, HIGH);
        if (pirState == LOW) {
            Serial.print("Motion detected: ");
            Serial.println(++count);
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

