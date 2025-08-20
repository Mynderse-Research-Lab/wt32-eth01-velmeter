#include "mqtt_utils.hpp"
#include <Arduino.h>
#include "config.hpp"
#include <velmeter_utils.hpp>

extern PubSubClient client;

// Attemps to reconnect
void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("WT32Client"))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 2s");
            delay(2000);
        }
    }
}

// Event: Print IP once obtained
void WiFiEvent(WiFiEvent_t event)
{
    if (event == ARDUINO_EVENT_ETH_GOT_IP)
    {
        Serial.print("IP: ");
        Serial.println(ETH.localIP());
    }
}

// Send a message every 5 seconds
void sanityCheck()
{

    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 5000)
    {
        lastMsg = millis();
        String msg = "hello from WT32";
        Serial.print("Publishing: ");
        Serial.println(msg);
        client.publish(MQTT_TOPIC_TEST, msg.c_str());
    }
}

void enableLAN8720A()
{
    pinMode(ETH_POWER_PIN, OUTPUT);
    digitalWrite(ETH_POWER_PIN, HIGH);
}

void velocityMsg(float rpm, float linearSpeed, float distanceTraveled, int direction)
{
    char msg[128];
    snprintf(msg, sizeof(msg),
            "RPM: %.2f | v: %.3f m/s | x: %.3f m | Dir: %s | CPR: %d | PC: %d",
            rpm, linearSpeed, distanceTraveled,
            (direction == 1 ? "Forward" : "Reverse"),
            totalPulseCount, pulseCount);

    
    client.loop();
    bool ok = client.publish(MQTT_TOPIC_TEST, msg);
    if(!ok)
    {
        Serial.println(strlen(msg));
        Serial.println("Failed to publish velocity message");
    }

    client.loop();

}
