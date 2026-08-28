#include <ETH.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include <Wire.h>
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Encoder.h>
#include "CellNetL2.h"
#include "EspEthL2Transport.h"
#include "EthernetLink.h"

// -------------------- ETH + MQTT SETUP --------------------

unsigned long lastPublish = 0;

// ---------- MQTT (Mosquitto) ----------
#define MQTT_HOST        "192.168.2.1"
#define MQTT_PORT        1883             // 1883 (no TLS), 8883 (TLS)
#define MQTT_USERNAME    ""               // set to "" if anonymous
#define MQTT_PASSWORD    ""               // set to "" if anonymous
#define MQTT_CLIENT_ID   "esp32-ether-01"
#define MQTT_SUB_TOPIC   "lab/inbox"
#define MQTT_PUB_TOPIC   "lab/outbox"

// Static IP (no router/DHCP)
IPAddress local_IP(192, 168, 2, 2);   // ESP32's static IP
IPAddress gateway(192, 168, 2, 1);    // set to PC/broker if no router
IPAddress subnet(255, 255, 255, 0);   // subnet mask
IPAddress dns(8, 8, 8, 8);            // DNS (not needed if using raw IPs)

// ---------- Reconnect timers ----------
Ticker mqttReconnectTimer;
Ticker ethRetryTimer;

AsyncMqttClient mqttClient;
bool netUp = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- MQTT callbacks ----------
void onMqttConnect(bool sessionPresent) {
  Serial.println("[MQTT] Connected.");
  mqttClient.subscribe(MQTT_SUB_TOPIC, 1);
  mqttClient.publish(MQTT_PUB_TOPIC, 1, true, "online (ethernet)");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("[MQTT] Disconnected (%d).\n", (int)reason);
  if (netUp) {
    mqttReconnectTimer.once(2, []() {
      mqttClient.connect();
    });
  }
}

void onMqttMessage(char* topic, char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" => ");
  for (size_t i = 0; i < len; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// ---------- Ethernet events ----------
void EthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[ETH] start");
      ETH.setHostname("esp32-eth");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[ETH] link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.printf("[ETH] IP: %s\n", ETH.localIP().toString().c_str());
      netUp = true;
      mqttClient.connect();
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[ETH] link down");
      netUp = false;
      mqttReconnectTimer.detach();
      ethRetryTimer.once(2, []() {
        ETH.begin(
          /*phy_addr*/1, /*power*/16, /*mdc*/23, /*mdio*/18,
          ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT
        );

        if (!ETH.config(local_IP, gateway, subnet, dns)) {
          Serial.println("[ETH] static IP re-config FAILED");
        } else {
          Serial.printf("[ETH] static IP re-set: %s\n",
                        ETH.localIP().toString().c_str());
        }
      });
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("[ETH] stop");
      netUp = false;
      break;
    default:
      break;
  }
}

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
  WiFi.onEvent(EthEvent);

  // MQTT setup
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  if (strlen(MQTT_USERNAME)) {
    mqttClient.setCredentials(MQTT_USERNAME, MQTT_PASSWORD);
  }
  mqttClient.setClientId(MQTT_CLIENT_ID);
  mqttClient.setKeepAlive(30);   // seconds
  mqttClient.setCleanSession(true);
  mqttClient.setWill(MQTT_PUB_TOPIC, 1, true, "offline");

  // Start Ethernet (WT32-ETH01 / LAN8720 typical pins/clock)
  ETH.begin(
    /*phy_addr*/1, /*power*/16, /*mdc*/23, /*mdio*/18,
    ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT
  );

  // apply static IP right after begin
  delay(50);
  if (!ETH.config(local_IP, gateway, subnet, dns)) {
    Serial.println("[ETH] static IP config FAILED");
  } else {
    Serial.printf("[ETH] static IP set: %s\n",
                  ETH.localIP().toString().c_str());
  }

  // ------- Encoder setup -------
  pinMode(encoderPinA, INPUT);//_PULLUP);
  pinMode(encoderPinB, INPUT);//_PULLUP);

  ESP32Encoder::useInternalWeakPullResistors = puType::none; //GPIO 36 and 39 do not have internal pull-ups (input only)
  //pcnt_unit_config_t::intr_priority 
  encoder.attachFullQuad(encoderPinA, encoderPinB);
  encoder.clearCount();
  lastTime = millis();
}

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
    float linearSpeed = revolutions * wheelCircumference / (millis()-previousTime) / 1000.0f;
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
    
    Serial.printf("A: %d B: %d\n", digitalRead(encoderPinA), digitalRead(encoderPinB));
    
    Serial.print("Count: ");
    Serial.println(currentCount);

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
    // ---- Optional: publish over MQTT ----
    if (mqttClient.connected()) {
      char msg[160];
      snprintf(msg, sizeof(msg),
               "{\"rpm\":%.2f,\"mps\":%.4f,\"distance\":%.3f,\"dir\":\"%s\"}",
               rpm,
               linearSpeed,
               distanceTraveled,
               (direction > 0 ? "FWD" : "REV"));
      snprintf(msg, sizeof(msg),
              "{\"rpm\":%.2f,\"mps\":%.4f,\"distance\":%.3f}",
              rpm,
              linearSpeed,
              distanceTraveled);
      mqttClient.publish("lab/wheel_velocity", 0, false, msg);
    }
    
  previousCount = currentCount;
  previousTime = currentTime;
  lastTime = currentTime;
  }

}

