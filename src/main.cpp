#include <ETH.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>

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
const int encoderPinA = 15;  // IO15
const int encoderPinB = 32;  // IO17

// Encoder counters
volatile int pulseCount      = 0;  // pulses in current interval
volatile int totalPulseCount = 0;  // total pulses (for distance)

// Time for velocity calculation
unsigned long lastTime = 0;       // last time interval was processed

// Encoder properties
const int countsPerRev = 2032;  // counts per wheel revolution (from your working code)

// Wheel properties
float wheelDiameterInches = 3.757;
float wheelCircumference  = (wheelDiameterInches * 0.0254f) * 3.14159f; // meters

// Direction: +1 for forward, -1 for reverse
volatile int direction = 1;

// Interrupt function to handle channel A change
void IRAM_ATTR handleChannelA() {
  // Determine direction by checking the state of channel B
  if (digitalRead(encoderPinA) == digitalRead(encoderPinB)) {
    direction = 1;   // Forward
  } else {
    direction = -1;  // Reverse
  }

  pulseCount      += direction;
  totalPulseCount += direction;
}

// Interrupt function to handle channel B change
void IRAM_ATTR handleChannelB() {
  // Determine direction by checking the state of channel A
  if (digitalRead(encoderPinA) != digitalRead(encoderPinB)) {
    direction = 1;   // Forward
  } else {
    direction = -1;  // Reverse
  }

  pulseCount      += direction;
  totalPulseCount += direction;
}

// -------------------- SETUP --------------------

void setup() {
  Serial.begin(115200);

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
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(encoderPinA), handleChannelA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), handleChannelB, CHANGE);

  lastTime = millis();
}

// -------------------- LOOP --------------------

void loop() {
  // Interval for velocity calculation
  const unsigned long interval = 250;  // 0.25 seconds
  unsigned long currentTime = millis();
  
  if (currentTime - lastTime >= interval) {
    float rpm         = 0.0f;
    float linearSpeed = 0.0f;  // m/s

    // Safely copy and reset pulseCount
    int pulsesSnapshot;
    noInterrupts();
    pulsesSnapshot = pulseCount;
    pulseCount     = 0;
    interrupts();

    if (pulsesSnapshot != 0) {
      float dt = interval / 1000.0f;  // seconds

      // Wheel rotations in this interval
      float wheelRotations = fabs((float)pulsesSnapshot) / (float)countsPerRev;

      // RPM (unsigned)
      rpm = (wheelRotations / dt) * 60.0f;

      // Linear speed in m/s (unsigned)
      linearSpeed = (rpm * wheelCircumference) / 60.0f;

      // Apply direction sign
      if (direction < 0) {
        rpm         = -rpm;
        linearSpeed = -linearSpeed;
      }
    }

    // Total distance traveled (meters)
    float distanceTraveled =
        ( (float)totalPulseCount / (float)countsPerRev ) * wheelCircumference;

    // ---- Print to Serial ----
    Serial.print("RPM: ");
    Serial.print(rpm);
    Serial.print(" | Linear Speed: ");
    Serial.print(linearSpeed);
    Serial.print(" m/s | Position: ");
    Serial.print(distanceTraveled);
    Serial.print(" m | CPR: ");
    Serial.print(totalPulseCount);
    Serial.print(" | Direction: ");
    Serial.println(direction == 1 ? "Forward" : "Reverse");

    // ---- Optional: publish over MQTT ----
    if (mqttClient.connected()) {
      char msg[160];
      snprintf(msg, sizeof(msg),
               "{\"rpm\":%.2f,\"mps\":%.4f,\"distance\":%.3f,\"dir\":\"%s\"}",
               rpm,
               linearSpeed,
               distanceTraveled,
               (direction > 0 ? "FWD" : "REV"));
      mqttClient.publish("lab/wheel_velocity", 0, false, msg);
    }

    lastTime = currentTime;
  }
}
