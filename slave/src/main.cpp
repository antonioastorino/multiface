#include <Arduino.h>

void setup(void) {
    Serial.begin(115200);
}

void loop(void) {
    Serial.print("This is a very long string but you should not crop it or wrap it or crap it! - ");
    Serial.print("This is a very long string but you should not crop it or wrap it or crap it! - ");
    Serial.println("This is a very long string but you should not crop it or wrap it or crap it!");
    delay(1000);
}
