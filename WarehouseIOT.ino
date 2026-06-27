#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "what";
const char* WIFI_PASS = "password";
const char* MQTT_SERVER = "broker.emqx.io";
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT = "office_floor1_monitoring";

const char* TOPIC_SYSTEM     = "office/floor1/system";
const char* TOPIC_SECURITY   = "office/floor1/security";
const char* TOPIC_IR         = "office/floor1/IRsensor";
const char* TOPIC_REEDA      = "office/floor1/reedA";
const char* TOPIC_DOOR       = "office/floor1/maindoor";
const char* TOPIC_VIBRATION  = "office/floor1/vibration";
const char* TOPIC_BUZZERA    = "office/floor1/buzzerA";

const int PIN_IR         = 25;
const int PIN_REEDA      = 14;
const int PIN_VIBRATION  = 27;
const int PIN_BUZZERA    = 26;

WiFiClient espClient;
PubSubClient mqtt(espClient);

int lastReedState = LOW;
int currentReedState = LOW;
int lastVibrationState = LOW;
int currentVibrationState = LOW;

bool objectDetected = false;
bool dangerState = false;
bool cooldownMode = false;
bool doorWarningLatched = false;
bool buzzerActive = false;

unsigned long vibrationWindowStart = 0;
unsigned long cooldownStart = 0;
unsigned long lastNotificationTime = 0;

int vibrationCount = 0;

const unsigned long VIBRATION_WINDOW = 5000;
const unsigned long COOLDOWN_DURATION = 10000;
const unsigned long NOTIFICATION_INTERVAL = 20000;

//========================
// WiFi Connection
//========================
void setupWiFi() {

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

//========================
// MQTT Publish
//========================
void publishEvent(const char* topic, const char* message) {

  mqtt.publish(topic, message);

  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);
}

//========================
// MQTT Reconnect
//========================
void reconnectMQTT() {

  while (!mqtt.connected()) {

    Serial.print("Connecting MQTT...");

    if (mqtt.connect(MQTT_CLIENT)) {

      Serial.println("CONNECTED");

      publishEvent(TOPIC_SYSTEM, "OFFICE FLOOR 1 ONLINE");

    }
    else {

      Serial.println("FAILED");

      delay(3000);

    }
  }
}

//========================
// Alarm ON
//========================
void dangerBuzzer() {

  if (!buzzerActive) {

    tone(PIN_BUZZERA, 2000);

    publishEvent(TOPIC_BUZZERA, "CONTINUOUS");

    buzzerActive = true;
  }
}

//========================
// Alarm OFF
//========================
void buzzerOff() {

  if (!dangerState && !doorWarningLatched) {

    noTone(PIN_BUZZERA);

    if (buzzerActive) {

      publishEvent(TOPIC_BUZZERA, "OFF");

      buzzerActive = false;
    }
  }
}
//====================================================
// IR Motion Sensor
//====================================================
void handleIRSensor() {

  static unsigned long lastIRChange = 0;
  int irState = digitalRead(PIN_IR);

  if (millis() - lastIRChange > 500) {

    if (irState == LOW && !objectDetected) {

      objectDetected = true;
      lastIRChange = millis();

      Serial.println("Object Detected");

      publishEvent(TOPIC_IR, "OBJECT DETECTED");
    }

    else if (irState == HIGH && objectDetected) {

      objectDetected = false;
      lastIRChange = millis();

      Serial.println("No Object");

      publishEvent(TOPIC_IR, "NO OBJECT DETECTED");
    }
  }
}

//====================================================
// Door Reed Switch
//====================================================
void handleReedSensor() {

  currentReedState = digitalRead(PIN_REEDA);

  if (currentReedState != lastReedState) {

    if (currentReedState == HIGH) {

      publishEvent(TOPIC_REEDA, "OPEN");
      publishEvent(TOPIC_DOOR, "OPEN");

      if (objectDetected) {

        Serial.println("Authorized Access");

        publishEvent(TOPIC_SECURITY, "AUTHORIZED ACCESS");

        dangerState = false;
        doorWarningLatched = false;

        buzzerOff();

      }
      else {

        Serial.println("WARNING : Door Open Without Detection");

        doorWarningLatched = true;

        publishEvent(TOPIC_SECURITY, "UNAUTHORIZED ACCESS");
        publishEvent(TOPIC_SYSTEM, "WARNING ACTIVE");

        dangerBuzzer();
      }
    }

    else {

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

//====================================================
// Vibration Sensor
//====================================================
void handleVibrationSensor() {

  if (cooldownMode) {

    if (millis() - cooldownStart >= COOLDOWN_DURATION) {

      cooldownMode = false;
      vibrationWindowStart = 0;
      vibrationCount = 0;
      dangerState = false;

      Serial.println("Cooldown Finished");

      publishEvent(TOPIC_SYSTEM, "COOLDOWN FINISHED");

      buzzerOff();
    }

    return;
  }

  currentVibrationState = digitalRead(PIN_VIBRATION);

  if (currentVibrationState == HIGH &&
      lastVibrationState == LOW) {

    unsigned long currentTime = millis();

    if (vibrationWindowStart == 0)
      vibrationWindowStart = currentTime;

    vibrationCount++;

    Serial.print("Vibration Count : ");
    Serial.println(vibrationCount);

    char vibrationMessage[30];
    sprintf(vibrationMessage, "COUNT_%d", vibrationCount);

    publishEvent(TOPIC_VIBRATION, vibrationMessage);

    if ((currentTime - vibrationWindowStart) <= VIBRATION_WINDOW) {

      if (vibrationCount < 8) {

        publishEvent(TOPIC_SYSTEM, "NORMAL VIBRATION");
      }

      if (vibrationCount >= 8 && !dangerState) {

        dangerState = true;

        Serial.println("Danger Vibration");

        publishEvent(TOPIC_SECURITY, "DANGER");
        publishEvent(TOPIC_SYSTEM, "ALARM ACTIVE");

        dangerBuzzer();

        cooldownMode = true;
        cooldownStart = millis();

        publishEvent(TOPIC_SYSTEM, "COOLDOWN ACTIVE");
      }
    }

    else {

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

//====================================================
// Setup
//====================================================
void setup() {

  Serial.begin(115200);

  pinMode(PIN_IR, INPUT);
  pinMode(PIN_REEDA, INPUT_PULLUP);
  pinMode(PIN_VIBRATION, INPUT);
  pinMode(PIN_BUZZERA, OUTPUT);

  noTone(PIN_BUZZERA);

  setupWiFi();

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("==================================");
  Serial.println("WareSafe Office Monitoring");
  Serial.println("Office Floor 1");
  Serial.println("==================================");

  publishEvent(TOPIC_SYSTEM, "NORMAL CONDITION");
  publishEvent(TOPIC_SECURITY, "SYSTEM ACTIVE");

  lastReedState = digitalRead(PIN_REEDA);
  lastVibrationState = digitalRead(PIN_VIBRATION);
}

//====================================================
// Main Loop
//====================================================
void loop() {

  if (!mqtt.connected()) {

    reconnectMQTT();
  }

  mqtt.loop();

  handleIRSensor();

  handleReedSensor();

  handleVibrationSensor();

  delay(50);
}