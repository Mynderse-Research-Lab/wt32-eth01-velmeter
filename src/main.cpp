// #include <ETH.h>
// #include <PubSubClient.h>
// #include "config.hpp"
// #include "mqtt_utils.hpp"
// #include "velmeter_utils.hpp"

// // MQTT client
// WiFiClient ethClient;
// PubSubClient client(ethClient);

// // Velocity meter
// unsigned long lastTime = 0;      // Last time we checked pulses
// unsigned long interval = 250; // 1 second
// unsigned long currentTime = millis();
// float wheelDiameterInches = 3.757; // Wheel properties
// float wheelCircumference = (wheelDiameterInches * 0.0254) * 3.14159; // Circumference in meters

// void setup()
// {
//   // Setting up serial comms
//           Serial.begin(115200);
//   delay(1000);
//   Serial.println("Starting ETH...");

//   // Setting up MQTT network
//   enableLAN8720A();

//   WiFi.onEvent(WiFiEvent);

//   // Setting up ethernet comms
//   ETH.begin(ETH_ADDR,
//             ETH_POWER_PIN,
//             ETH_MDC_PIN,
//             ETH_MDIO_PIN,
//             ETH_TYPE,
//             ETH_CLK_MODE);

//   IPAddress local_ip(192, 168, 2, 2);
//   IPAddress gateway(192, 168, 2, 1);
//   IPAddress subnet(255, 255, 255, 0);
//   ETH.config(local_ip, gateway, subnet);

//   client.setServer(MQTT_SERVER, MQTT_PORT);

//   // Setting up Velocity meter
//   pinMode(ENCORDER_A_PIN, INPUT);
//   pinMode(ENCORDER_B_PIN, INPUT);

//   // Attach interrupts to both encoder channels
//   attachInterrupt(digitalPinToInterrupt(ENCORDER_A_PIN), handleChannelA, CHANGE);
//   attachInterrupt(digitalPinToInterrupt(ENCORDER_B_PIN), handleChannelB, CHANGE);

//   lastTime = millis();
// }

// void loop()
// {
//   // checks connection
//   if (!client.connected())
//   {
//     reconnect();
//   }

//   // Maintains the connection to the broker-- call often
//   client.loop();

//   // Send a message every 5 seconds
//   //sanityCheck();


//   unsigned long currentTime = millis();
//   if (currentTime - lastTime >= interval) {

//     lastTime = currentTime;

//     // Calculate RPM and linear speed only if there was motion
//     float rpm = 0.0;
//     float linearSpeed = 0.0;

//     if (pulseCount != 0) {
//       // Calculate RPM based on wheel revolutions
//       float wheelRotations = abs(pulseCount) / float(countsPerRev);
//       rpm = (wheelRotations / (interval / 1000.0)) * 60.0;

//       // Calculate linear speed in meters per second
//       linearSpeed = (rpm * wheelCircumference) / 60.0;  // m/s
//     }
    
//     // Calculate total distance traveled for position
//     float distanceTraveled = (totalPulseCount / float(countsPerRev)) * wheelCircumference; // meters
   
//     // Publish the velocity message
//     velocityMsg(rpm, linearSpeed, distanceTraveled, direction);

//     // Reset pulse count for next interval
//     pulseCount = 0;
  
    
//   }

// }



#include <Arduino.h>
// #include <LiquidCrystal.h>  // you can leave it included, but it's unused
#include <ETH.h>
#include <PubSubClient.h>
#include "config.hpp"
#include "mqtt_utils.hpp"
#include "velmeter_utils.hpp"

// MQTT client
WiFiClient ethClient;
PubSubClient client(ethClient);

unsigned long lastTime    = 0;

// const int countsPerRev = 2032;                 
// const int pulsesPerEncoderRevolution = 500;    
// const float gearRatio = 15.0f;
float wheelDiameterInches = 3.757f;
float wheelCircumference = (wheelDiameterInches * 0.0254f) * 3.14159f;

void setup() {
  // Setting up serial comms
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting ETH...");

  // Setting up MQTT network
  enableLAN8720A();

  WiFi.onEvent(WiFiEvent);

  // Setting up ethernet comms
  ETH.begin(ETH_ADDR,
            ETH_POWER_PIN,
            ETH_MDC_PIN,
            ETH_MDIO_PIN,
            ETH_TYPE,
            ETH_CLK_MODE);

  IPAddress local_ip(192, 168, 2, 2);
  IPAddress gateway(192, 168, 2, 1);
  IPAddress subnet(255, 255, 255, 0);
  ETH.config(local_ip, gateway, subnet);

  ethClient.setNoDelay(true);    // smooth out TCP (reduces burstiness)
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setBufferSize(1024);    // increase if your payloads are bigger
  client.setKeepAlive(15);       // seconds
  client.setSocketTimeout(2);    // seconds

  // Setting up Velocity meter
  pinMode(encoderPinA, INPUT);
  pinMode(encoderPinB, INPUT);

  // Attach interrupts to both encoder channels
  attachInterrupt(digitalPinToInterrupt(encoderPinA), handleChannelA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), handleChannelB, CHANGE);


  lastTime = millis();
}

void loop() {

  // checks connection
  if (!client.connected())
  {
    reconnect();
  }

  // Maintains the connection to the broker-- call often
  client.loop();


  // Define interval to calculate speed (e.g., every 1 second)
  unsigned long interval = 250; // 1 second
  unsigned long currentTime = millis();
  
  if (currentTime - lastTime >= interval) {
    // Calculate RPM and linear speed only if there was motion
    float rpm = 0.0;
    float linearSpeed = 0.0;

    if (pulseCount != 0) {
      // Calculate RPM based on wheel revolutions
      float wheelRotations = abs(pulseCount) / float(countsPerRev);
      rpm = (wheelRotations / (interval / 1000.0)) * 60.0;

      // Calculate linear speed in meters per second
      linearSpeed = (rpm * wheelCircumference) / 60.0;  // m/s
    }
    
    // Calculate total distance traveled for position
    float distanceTraveled = (totalPulseCount / float(countsPerRev)) * wheelCircumference; // meters
    
    // Print RPM, linear speed, and position (distance traveled) to Serial Monitor
    // Serial.print("RPM: ");
    // Serial.print(rpm);
    // Serial.print(" | Linear Speed: ");
    // Serial.print(linearSpeed);
    // Serial.print(" m/s | Position: ");
    // Serial.print(distanceTraveled);
    // Serial.print(" CPR: ");
    // Serial.print(totalPulseCount);
    // Serial.print(" m | Direction: ");
    // Serial.println(direction == 1 ? "Forward " : "Reverse ");

    // Publish the velocity message
    velocityMsg(rpm, linearSpeed, distanceTraveled, direction);
    // Maintains the connection to the broker-- call often
    client.loop();
    int room = ethClient.availableForWrite();
    Serial.print("Available for write: ");

   // Reset pulse count for next interval
    pulseCount = 0;
    lastTime = currentTime;
  }
}
