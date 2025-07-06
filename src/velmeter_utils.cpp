#include "config.hpp"
#include "velmeter_utils.hpp"

// Variable to track direction: +1 for forward, -1 for reverse
volatile int direction = 1;
volatile int pulseCount = 0;     // Count of pulses within each interval
volatile int totalPulseCount = 0;// Total count of pulses for position tracking

// Interrupt function to handle channel A change
void handleChannelA() {
    // Determine direction by checking the state of channel B
    if (digitalRead(ENCORDER_A_PIN) == digitalRead(ENCORDER_B_PIN)) {
      direction = 1;  // Forward
    } else {
      direction = -1; // Reverse
    }
    
    // Update pulse counts based on direction
    pulseCount += direction;
    totalPulseCount += direction;
} 

// Interrupt function to handle channel B change
void handleChannelB() {
    // Determine direction by checking the state of channel A
    if (digitalRead(ENCORDER_A_PIN) != digitalRead(ENCORDER_B_PIN)) {
      direction = 1;  // Forward
    } else {
      direction = -1; // Reverse
    }
  
    // Update pulse counts based on direction
    pulseCount += direction;
    totalPulseCount += direction;
  }

