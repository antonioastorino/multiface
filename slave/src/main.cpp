#include <Arduino.h>

void setup(void)
{
    Serial.begin(115200);
    Serial.setTimeout(5000);
}

void loop(void)
{
    String receivedData = Serial.readStringUntil('\n');
    if (receivedData.length() <= 0) {
        return;
    }
    if (strncmp(receivedData.c_str(), "give me a long string!", strlen("give me a long string!"))
        == 0)
    {
        Serial.print(
            "This is a very long string but you should not crop it or wrap it or crap it! - ");
        Serial.print(
            "This is a very long string but you should not crop it or wrap it or crap it! - ");
        Serial.println(
            "This is a very long string but you should not crop it or wrap it or crap it!");
    }
    else
    {
        Serial.print("Invalid command `");
        Serial.print(receivedData);
        Serial.println("`");
    }
}
