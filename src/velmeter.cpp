/* #include <Servo.h>
#include <LiquidCrystal.h>

// Define LCD pins and initialize LiquidCrystal object
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 9, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


volatile int pulseCount = 0;     // Count of pulses within each interval
volatile int totalPulseCount = 0;// Total count of pulses for position tracking
unsigned long lastTime = 0;      // Last time we checked pulses


// Wheel properties
float wheelDiameterInches = 3.757;
float wheelCircumference = (wheelDiameterInches * 0.0254) * 3.14159; // Circumference in meters

// Variable to track direction: +1 for forward, -1 for reverse
volatile int direction = 1;

// Interrupt function to handle channel A change
void handleChannelA() {
  // Determine direction by checking the state of channel B
  if (digitalRead(encoderPinA) == digitalRead(encoderPinB)) {
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
  if (digitalRead(encoderPinA) != digitalRead(encoderPinB)) {
    direction = 1;  // Forward
  } else {
    direction = -1; // Reverse
  }

  // Update pulse counts based on direction
  pulseCount += direction;
  totalPulseCount += direction;
}

void setup() {
  Serial.begin(9600);
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.print("Initializing...");

  // Initialize encoder pins
  pinMode(encoderPinA, INPUT);
  pinMode(encoderPinB, INPUT);
  
  // Attach interrupts to both encoder channels
  attachInterrupt(digitalPinToInterrupt(encoderPinA), handleChannelA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), handleChannelB, CHANGE);
  
  lastTime = millis();
}

void loop() {
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
    Serial.print("RPM: ");
    Serial.print(rpm);
    Serial.print(" | Linear Speed: ");
    Serial.print(linearSpeed);
    Serial.print(" m/s | Position: ");
    Serial.print(distanceTraveled);
    Serial.print(" CPR: ");
    Serial.print(totalPulseCount);
    Serial.print(" m | Direction: ");
    Serial.println(direction == 1 ? "Forward " : "Reverse ");
   
    
    
    // Update LCD Display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Speed: ");
    lcd.print(linearSpeed, 2);  // Display linear speed with 2 decimal places
    lcd.print(" m/s");
    
    lcd.setCursor(0, 1);
    lcd.print("Pos: ");
    lcd.print(distanceTraveled, 2);  // Display position with 2 decimal places
    lcd.print(" m");
   
    // Reset pulse count for next interval
    pulseCount = 0;
    lastTime = currentTime;
  }
} */