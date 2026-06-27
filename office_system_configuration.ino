// code office_system_configuration
#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "what";
const char* WIFI_PASS = "password";
const char* MQTT_SERVER = "broker.emqx.io";
const int MQTT_PORT = 1883;

const char* MQTT_CLIENT = "office_floor1_monitoring"; 

const char* TOPIC_SYSTEM = "office/floor1/system";
const char* TOPIC_SECURITY = "office/floor1/security";
const char* TOPIC_ATTACK = "office/floor1/attack";
const char* TOPIC_IR = "office/floor1/IRsensor";
const char* TOPIC_REEDA = "office/floor1/reedA";
const char* TOPIC_DOOR = "office/floor1/maindoor";
const char* TOPIC_VIBRATION = "office/floor1/vibration";
const char* TOPIC_BUZZERA = "office/floor1/buzzerA";

const int PIN_IR = 25;
const int PIN_REEDA = 14;
const int PIN_VIBRATION = 27;
const int PIN_BUZZERA = 26;

WiFiClient espClient;
PubSubClient mqtt(espClient);

int lastReedState = LOW;
int currentReedState = LOW;

int lastVibrationState = LOW;
int currentVibrationState = LOW;

bool objectDetected = false;
bool dangerState = false;
bool cooldownMode = false;
bool isCompromised = false;
bool isDefenseActive = false;

bool doorWarningLatched = false;
bool buzzerActive = false;

unsigned long vibrationWindowStart = 0;
unsigned long cooldownStart = 0;
unsigned long lastNotificationTime = 0;

int vibrationCount = 0;

const unsigned long VIBRATION_WINDOW = 5000;
const unsigned long COOLDOWN_DURATION = 10000;
const unsigned long NOTIFICATION_INTERVAL = 20000;

void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting WiFi : ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());
}

void publishEvent(const char* topic, const char* message) {
  mqtt.publish(topic, message);

  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting MQTT...");

    if (mqtt.connect(MQTT_CLIENT)) {
      Serial.println("CONNECTED");
      publishEvent(TOPIC_SYSTEM, "OFFICE FLOOR 1 ONLINE");

    } else {
      Serial.println("FAILED");
      delay(3000);
    }
  }
}

void dangerBuzzer() {
  if (!buzzerActive) {
    tone(PIN_BUZZERA, 2000);
    publishEvent(TOPIC_BUZZERA, "CONTINUOUS");
    buzzerActive = true;
  }
}

void buzzerOff() {
  if (!dangerState && !doorWarningLatched) {
    noTone(PIN_BUZZERA);

    if (buzzerActive) {
      publishEvent(TOPIC_BUZZERA, "OFF");
      buzzerActive = false;
    }
  }
}

void runReplayAttackScenario1() {
  isCompromised = true;

  publishEvent(TOPIC_ATTACK, "SCENARIO_1_STARTED");
  publishEvent(TOPIC_SECURITY, "SCENARIO_1_DETECTED");
  delay(1000);

  if(isDefenseActive && digitalRead(PIN_IR) == HIGH) {
    isCompromised = false;
    Serial.println("[DEFENSE] SCENARIO 1 BLOCKED");
    publishEvent(TOPIC_IR, "REPLAY_DATA_REJECTED");
    publishEvent(TOPIC_DOOR, "CLOSED");
    publishEvent(TOPIC_SECURITY, "ATTACK_BLOCKED");
    publishEvent(TOPIC_ATTACK, "SCENARIO_1_BLOCKED");
    publishEvent(TOPIC_SYSTEM, "DEFENSE_WON");
    tone(PIN_BUZZERA, 3000, 300);
  } else {
    publishEvent(TOPIC_IR, "FAKE OBJECT DETECTED");
    delay(1000);
    publishEvent(TOPIC_DOOR, "OPEN");
    delay(1000);
    publishEvent(TOPIC_SYSTEM, "FAKE_ACCESS_REPLAYED");
    delay(1000);
    publishEvent(TOPIC_SECURITY, "SYSTEM_TRICKED");
  }
}

void runVibrationSpoofingAttackScenario2() {
  isCompromised = true;
  publishEvent(TOPIC_ATTACK, "SCENARIO_2_STARTED");
  publishEvent(TOPIC_SECURITY, "SCENARIO_2_DETECTED");
  delay(1000);

  if (isDefenseActive && digitalRead(PIN_VIBRATION == LOW)) {
    isCompromised = false;
    Serial.println("[DEFENSE] SCENARIO 2 BLOCKED");
    publishEvent(TOPIC_VIBRATION, "SPOOFED_DATA_REJECTED");
    publishEvent(TOPIC_SECURITY, "ATTACK_BLOCKED");
    publishEvent(TOPIC_SYSTEM, "DEFENSE_WON");
    tone(PIN_BUZZERA, 3000, 300);

    return;
  }
  
  for (int i = 0; i < 10; i++) {
    Serial.print("[ATTACK] Injecting vibration packet ");
    Serial.println(i + 1);

    publishEvent(TOPIC_VIBRATION, "ABNORMAL VIBRATION");
    publishEvent(TOPIC_SECURITY, "FALSE_VIBRATION_INJECTION");
    delay(300);
  }
  
  publishEvent(TOPIC_SYSTEM, "VIBRATION_SPOOFING_DETECTED");

  dangerState = true;
  doorWarningLatched = true;
  Serial.println("Danger Alarm");
  publishEvent(TOPIC_SECURITY, "DANGER CONDITION");
  publishEvent(TOPIC_SYSTEM, "DANGER ACTIVE");
  dangerBuzzer();
}

void resetSystemNormal() {
  objectDetected = false;
  dangerState = false;
  cooldownMode = false;
  isCompromised = false;
  isDefenseActive = false;
  doorWarningLatched = false;
  buzzerActive = false;

  vibrationCount = 0;
  vibrationWindowStart = 0;

  noTone(PIN_BUZZERA);
  publishEvent(TOPIC_BUZZERA, "OFF");
  Serial.println("[SYSTEM] NORMAL MODE RESTORED");
  publishEvent(TOPIC_SYSTEM, "NORMAL MODE RESTORED");
}

void handleSerialCommand() {
  if (Serial.available()) {
    char input = Serial.read();
    input = toupper(input);

    while (Serial.available()) {
      Serial.read();
    }
 
    if (input =='R') {
      runReplayAttackScenario1();
    }

    else if (input == 'V') {
      runVibrationSpoofingAttackScenario2();
    }

    else if (input == 'D') {
      isDefenseActive = !isDefenseActive;

      Serial.print("[SYSTEM] Defense Mode : ");

      Serial.println(isDefenseActive ? "ACTIVE" : "DISABLED");

      publishEvent(TOPIC_SYSTEM, isDefenseActive ? "DEFENSE_MODE_ACTIVE" : "DEFENSE_MODE_DISABLED");
    }
    
    else if (input == 'X') {
      resetSystemNormal();
    }
  }
}

void handleIRSensor() {
  if (isCompromised) return;
  static unsigned long lastIRChange = 0;
  int irState = digitalRead(PIN_IR);

  if (millis() - lastIRChange > 500) {
    if (irState == LOW && !objectDetected) {
      objectDetected = true;
      lastIRChange = millis();

      publishEvent(TOPIC_IR, "OBJECT DETECTED");
    }
    else if (irState == HIGH && objectDetected) {
      objectDetected = false;
      lastIRChange = millis();

      publishEvent(TOPIC_IR, "NO OBJECT DETECTED");
    }
  }
}

void handleReedSensor() {
  if (isCompromised) return;
  currentReedState = digitalRead(PIN_REEDA);

  if (currentReedState != lastReedState) {
    if (currentReedState == HIGH) {
      publishEvent(TOPIC_REEDA, "OPEN");
      publishEvent(TOPIC_DOOR, "OPEN");

      if (objectDetected) {
        Serial.println("Door Opened Legally");
        publishEvent(TOPIC_SECURITY, "AUTHORIZED ACCESS");
        buzzerOff();

      } else {
        doorWarningLatched = true;
        Serial.println("Door Open Warning");
        publishEvent(TOPIC_SECURITY, "DOOR OPEN WARNING");
        publishEvent(TOPIC_SYSTEM, "WARNING ACTIVE");
        dangerBuzzer();
      }

    } else {
      publishEvent(TOPIC_REEDA, "CLOSED");
      publishEvent(TOPIC_DOOR, "CLOSED");
      Serial.println("Door Closed");

      if (!doorWarningLatched) {
        buzzerOff();
      }
    }
    lastReedState = currentReedState;
  }
}

void handleVibrationSensor() {
  if (isCompromised) return;

  if (cooldownMode) {
    if (millis() - cooldownStart >= COOLDOWN_DURATION) {
      cooldownMode = false;
      vibrationWindowStart = 0;
      vibrationCount = 0;

      Serial.println("Cooldown Finished");
      publishEvent(TOPIC_SYSTEM, "COOLDOWN FINISHED");
    }
    return;
  }

  currentVibrationState = digitalRead(PIN_VIBRATION);
  
  if (currentVibrationState == HIGH && lastVibrationState == LOW) {
    unsigned long currentTime = millis();

    if (vibrationWindowStart == 0) {
      vibrationWindowStart = currentTime;
    }
    vibrationCount++;
    Serial.print("Vibration Count : ");
    Serial.println(vibrationCount);

    char vibrationMessage[50];
    sprintf(vibrationMessage, "COUNT_%d", vibrationCount);
    publishEvent(TOPIC_VIBRATION, vibrationMessage);

    if ((currentTime - vibrationWindowStart) <= VIBRATION_WINDOW) {
      if (vibrationCount < 8) {
        publishEvent(TOPIC_SYSTEM, "NORMAL VIBRATION");
      }

      if (vibrationCount >= 8 && !dangerState) {
        dangerState = true;
        Serial.println("Danger Vibration Detected");
        publishEvent(TOPIC_SECURITY, "DANGER ACTIVE");
        publishEvent(TOPIC_SYSTEM, "ALARM ACTIVE");
        dangerBuzzer();

        cooldownMode = true;
        cooldownStart = millis();

        publishEvent(TOPIC_SYSTEM, "COOLDOWN ACTIVE");
      }

    } else {
      vibrationWindowStart = currentTime;
      vibrationCount = 1;

      if (millis() - lastNotificationTime >= NOTIFICATION_INTERVAL) {
        publishEvent(TOPIC_SYSTEM, "WINDOW RESET");
        lastNotificationTime = millis();
      }
    }
  }

  lastVibrationState = currentVibrationState;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_REEDA, INPUT_PULLUP);
  pinMode(PIN_VIBRATION, INPUT);
  pinMode(PIN_BUZZERA, OUTPUT);
  pinMode(PIN_IR, INPUT);
  noTone(PIN_BUZZERA);

  setupWiFi();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("WareSafe Floor 1 Online");
  Serial.println("R = Replay Attack Scenario 1");
  Serial.println("V = Vibration Spoofing Scenario 2");
  Serial.println("D = Toggle Defense Mode");
  Serial.println("X = Reset System");

  mqtt.publish(TOPIC_SYSTEM, "NORMAL CONDITION");
  mqtt.publish(TOPIC_SECURITY, "SYSTEM NORMAL");

  lastReedState = digitalRead(PIN_REEDA);
  lastVibrationState = digitalRead(PIN_VIBRATION);
}

void loop() {
  if (!mqtt.connected()) {
    reconnectMQTT();
  }

  mqtt.loop();

  handleSerialCommand();
  handleIRSensor();
  handleReedSensor();
  handleVibrationSensor();
  delay(50);
}

