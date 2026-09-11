//#include <ETH.h>
//#include <AsyncMqttClient.h>
//#include <Ticker.h>
#include <Wire.h>
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Encoder.h>
#include "CellNetL2.h"
#include "EspEthL2Transport.h"
#include "EthernetLink.h"

bool DoSerialPrint = true;

// -------------------- SETUP --------------------
unsigned long lastPublish = 0;

bool netUp = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);

static Network::EthernetLink s_eth_link;
static CellNet::EspEthL2Transport s_l2_transport;
static CellNet::CellNetL2Node s_l2_node(s_l2_transport, CellNodeId::CONVEYOR);

static bool cellNetReady = false;
int print_statement_counter=0;


// -------------------- ENCODER / VELOCITY SETUP --------------------

// Encoder pins (avoid GPIO17 – used by ETH clock)
const int encoderPinA = 36;//15;  // IO15
const int encoderPinB = 39;//32;  // IO17
constexpr int encoderPINA=36;
constexpr int encoderPINB=39;

ESP32Encoder encoder;

// Encoder counters
volatile int pulseCount      = 0;  // pulses in current interval
volatile int totalPulseCount = 0;  // total pulses (for distance)
volatile int totalPulsesSnapshot = 0;

// Time for velocity calculation
unsigned long lastTime = 0;       // last time interval was processed

// Encoder properties
const int countsPerRev = 500*4;//500;//2032;  // counts per wheel revolution (from your working code) //pulses per rev in quadrature

// Wheel properties
float wheelDiameterInches = 3.757;
float wheelDiameterMeters = wheelDiameterInches / 39.37f;
float wheelCircumference  = 0.299708;//wheelDiameterMeters * 3.14159f; // meters


// Direction: +1 for forward, -1 for reverse
volatile int direction = 1;
bool sent = false;


// -------------------- SETUP --------------------

void setup() {
  Serial.begin(115200);

  //setup LCD display
  Wire.begin(4,14); //SDA = IO4, SCL = IO14

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ready");

  // ETH event hook
  //WiFi.onEvent(EthEvent);

  // ------- Encoder setup -------
  pinMode(encoderPinA, INPUT);//_PULLUP);
  pinMode(encoderPinB, INPUT);//_PULLUP);

  ESP32Encoder::useInternalWeakPullResistors = puType::none; //GPIO 36 and 39 do not have internal pull-ups (input only)
  //pcnt_unit_config_t::intr_priority 
  encoder.attachFullQuad(encoderPinA, encoderPinB);
  encoder.clearCount();
  lastTime = millis();
  Serial.println("[CellNetL2] Starting Ethernet");
  if (!s_eth_link.start()){
    Serial.println("Ethernet Startup Failed");
    /*lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Ethernet Failed");*/
    return;
  }
  Serial.println("[CellNetL2] Waiting for Link");
  if(!s_eth_link.waitForUp(5000)){
    Serial.println("[CellNetL2] Link is not ready");
    /*lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("No ETH Link");
    delay(250);
    lcd.clear();*/
    return;
  }
  s_l2_transport.attachEthHandle(s_eth_link.getEthHandle());
  if (!s_l2_node.begin()) {
    Serial.println("[CellNetL2] Node initialization failed");
    /*lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CellNet failed");
    delay(250);
    lcd.clear();*/
    return;
  }
  cellNetReady = true;
  Serial.println("[CellNetl2] Conveyor node 0x02 ready");
  /*lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Network Ready");
  delay(500);
  lcd.clear();*/
}
 /*
void app_main(){
  s_eth_link.start();
  if (s_eth_link.waitForUp(5000)){
    s_l2_transport.attachEthHandle(S_eth_link.getEthHandle());
    s_l2_node.begin();
  }
  s_l2_node.onCellCommand([](const L2CellCommandPayLoad &cmd, const L2CellHeader &hdr){
    if(cmd.command_id == 1) startBelt();
    else if (cmd.command_id == 2) stopBelt();
  });

  xTaskCreatePinnedToCore(telemetryTask, "ConvTx", 4096, nullptr, 5, nullptr, 0);
}*/
// -------------------- LOOP --------------------

void loop() {
  // Interval for velocity calculation
  const unsigned long interval = 250;  // 0.25 seconds
  unsigned long currentTime = millis();

  static int64_t previousCount = 0;
  static uint32_t previousTime = millis();
  //constexpr float wheelCircumference = 0.300f;

  //currentTime = millis();
  //float distanceTraveled = (static_cast<float>(encoder.getCount()) / static_cast<float>(countsPerRev)) * wheelCircumference;
  
  if (millis() - previousTime >= interval) {
    int64_t currentCount = encoder.getCount();
    //int64_t countChange = currentCount - previousCount;
    //float elapsedSeconds = (currentTime-previousTime) / 1000.0f;

    float revolutions = static_cast<float>(encoder.getCount() - previousCount) / static_cast<float>(countsPerRev);
    float rpm = revolutions * 60.0f / (millis()-previousTime);// / 1000.0f;
    float linearSpeed = revolutions * wheelCircumference / (millis()-previousTime); /// 1000.0f;
    float distanceTraveled = (static_cast<float>(encoder.getCount()) / static_cast<float>(countsPerRev)) * wheelCircumference;
    //float rpm         = 0.0f;
    //float linearSpeed = 0.0f;  // m/s

    // ---- Print to Serial ----
    /*Serial.print("RPM: ");
    Serial.print(rpm);
    Serial.print(" | Linear Speed: ");
    Serial.print(linearSpeed);
    Serial.print(" m/s | Position: ");
    Serial.print(distanceTraveled);
    Serial.print(" m | CPR: ");
    Serial.print(totalPulsesSnapshot);*/

    //Serial.print("A: ");
    //Serial.print();
    
    if (DoSerialPrint) {
      Serial.printf("A: %d B: %d\n", digitalRead(encoderPinA), digitalRead(encoderPinB));
    
      Serial.print("Count: ");
      Serial.println(currentCount);
    }

    /*Serial.print(" | RPM: ");
    Serial.print(rpm);
    Serial.print(" | Speed: ");
    Serial.print(linearSpeed);
    //Serial.print(" | Direction: ");
    //Serial.println(direction == 1 ? "Forward" : "Reverse");
    Serial.println();*/
    lcd.setCursor(0,0);
    lcd.print("Position:");
    lcd.setCursor(0,1);
    lcd.print(distanceTraveled,2);
    lcd.print(" m     ");
    
   /*
    lcd.print("Pulses:");
    lcd.print(pulsesSnapshot);
    lcd.print("     ");
    lcd.setCursor(0,1);
    lcd.print(linearSpeed, 4);
    lcd.print(" m/s     ");*/
    //lcd.println("RPM: ", rpm);
    //lcd.print("Direction: ", direction == 1 ? "Forward" : "Reverse");
    
    //bool sent = false;
    float speedMmS = linearSpeed *1000.0f; //convert speed from m/s to mm/s?
    if (cellNetReady){

      bool sent = s_l2_node.sendConveyorSpeed(speedMmS, distanceTraveled, currentCount);
      //print_statement_counter++;
      if (print_statement_counter%4 == 0)
      {
        if (DoSerialPrint){
        Serial.println("[CellNetL2] message sent!");
        }
      }
      print_statement_counter++;
    }
    if(!sent && DoSerialPrint){
      Serial.println("[CellNetL2] CONVEYOR_SPEED send failure");
    }
    
    previousCount = currentCount;
    previousTime = currentTime;
    lastTime = currentTime;
  }

}

