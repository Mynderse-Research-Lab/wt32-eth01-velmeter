#pragma once

#include <Arduino.h>

extern volatile int direction;
extern volatile int pulseCount;
extern volatile int totalPulseCount;

// Interrupt function to handle channel A change
void handleChannelA();

// Interrupt function to handle channel B change
void handleChannelB();