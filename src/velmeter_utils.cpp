// #include "config.hpp"
// #include "velmeter_utils.hpp"
// #include "mqtt_utils.hpp"

// //extern PubSubClient client;

// // Variable to track direction: +1 for forward, -1 for reverse
// volatile int direction = 1;
// volatile int pulseCount = 0;     // Count of pulses within each interval
// volatile int totalPulseCount = 0;// Total count of pulses for position tracking

// // Interrupt function to handle channel A change
// void IRAM_ATTR handleChannelA() {  
//   if (digitalRead(encoderPinA) == digitalRead(encoderPinB)) {
//     direction = 1;
//   } else {
//     direction = -1;
//   }
//   pulseCount += direction;
//   totalPulseCount += direction;
// }

// // Interrupt function to handle channel B change
// void IRAM_ATTR handleChannelB() {  
//   if (digitalRead(encoderPinA) != digitalRead(encoderPinB)) {
//     direction = 1;
//   } else {
//     direction = -1;
//   }
//   pulseCount += direction;
//   totalPulseCount += direction;
// }

