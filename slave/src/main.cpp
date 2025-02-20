#include <Arduino.h>

void setup(void) {
    Serial.begin(115200);
}

void loop(void) {
    Serial.println("hello");
    delay(1000);
}
