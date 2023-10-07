int ledPin = 2;
int inputPin = 5;
int pirState = LOW;
int val = 0;
int count = 0;

void setup() {
    pinMode(ledPin, OUTPUT);
    pinMode(inputPin, INPUT);

    Serial.begin(9600);
}

void loop() {
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