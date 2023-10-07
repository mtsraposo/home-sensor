#include <ESP8266WiFi.h>

int ledPin = 2;
int inputPin = 5;
int pirState = LOW;
int val = 0;
int count = 0;

void await_connection() {
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
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
