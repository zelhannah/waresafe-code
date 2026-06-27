#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char* WIFI_SSID = "Redmi Note 11";
const char* WIFI_PASS = "1234567890";
const char* MQTT_SERVER = "broker.emqx.io";
const int MQTT_PORT = 1883;

const char* TOPIC_SYSTEM = "warehouse/floor2/system";
const char* TOPIC_SECURITY = "warehouse/floor2/security";
const char* TOPIC_ACCESS = "warehouse/floor2/access";
const char* TOPIC_REEDB = "warehouse/floor2/reedB";
const char* TOPIC_WAREHOUSEDOOR = "warehouse/floor2/warehousedoor";
const char* TOPIC_LIGHT = "warehouse/floor2/ledlight";
const char* TOPIC_BUZZERB = "warehouse/floor2/buzzerB";
const char* TOPIC_LCD = "warehouse/floor2/displaytext";
const char* TOPIC_ALARM = "warehouse/floor2/alarm";

#define SS_PIN 5
#define RST_PIN 27
#define REED_SWITCH 4
#define LED_GREEN 25
#define LED_YELLOW 26
#define LED_RED 33
#define BUZZERB 14

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient espClient;
PubSubClient client(espClient);

String allowedUID = "7729CD05";

bool accessGranted = false;
bool alarmActive = false;
bool forcedDoorAlarm = false;


int lastDoorStatus = LOW;


void publishEvent(const char* topic, String message) {
  client.publish(topic, message.c_str());
  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);
}




void updateLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);




  publishEvent(TOPIC_LCD, line1);
  publishEvent(TOPIC_LCD, line2);
}




void setupWiFi() {
  Serial.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);




  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }




  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());
}




void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");




    if (client.connect("warehouse_floor2_monitor")) {
      Serial.println("CONNECTED");
      publishEvent(TOPIC_SYSTEM, "WAREHOUSE FLOOR 2 ONLINE");




    } else {
      Serial.println("FAILED");
      delay(2000);
    }
  }
}


void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();




  pinMode(REED_SWITCH, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZERB, OUTPUT);




  lcd.init();
  lcd.backlight();
  updateLCD("WareSafe Ready", "Scan RFID");




  setupWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  delay(2000);




  updateLCD("System ACTIVE", "Scan RFID");
  publishEvent(TOPIC_SYSTEM, "NORMAL CONDITION");
  publishEvent(TOPIC_SECURITY, "SYSTEM ACTIVE");




  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  noTone(BUZZERB);




  lastDoorStatus = digitalRead(REED_SWITCH);
}




void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }




  client.loop();




  if (rfid.PICC_IsNewCardPresent() &&
      rfid.PICC_ReadCardSerial()) {




    String cardUID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) {
        cardUID += "0";
      }




      cardUID += String(rfid.uid.uidByte[i], HEX);
    }




    cardUID.toUpperCase();




    Serial.print("RFID UID: ");
    Serial.println(cardUID);




   
      Serial.println("ACCESS GRANTED");




      accessGranted = true;
      alarmActive = false;
      forcedDoorAlarm = false;




      noTone(BUZZERB);
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);




      publishEvent(TOPIC_LIGHT, "ON");
      updateLCD("ACCESS GRANTED", "Door Ready");
      publishEvent(TOPIC_ACCESS, "AUTHORIZED ACCESS");
      publishEvent(TOPIC_SECURITY, "SAFE ACCESS");
      publishEvent(TOPIC_ALARM, "OFF");
      publishEvent(TOPIC_BUZZERB, "OFF");
      publishEvent(TOPIC_WAREHOUSEDOOR, "READY TO OPEN");
      delay(1500);
    }




    else {
      Serial.println("ACCESS DENIED");




      accessGranted = false;
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, HIGH);
      digitalWrite(LED_RED, LOW);
      publishEvent(TOPIC_LIGHT, "ON");




      updateLCD("ACCESS DENIED", "Warning");
      publishEvent(TOPIC_ACCESS, "UNAUTHORIZED ACCESS");
      publishEvent(TOPIC_SECURITY, "WARNING");
      publishEvent(TOPIC_ALARM, "WARNING ACTIVE");
      publishEvent(TOPIC_BUZZERB, "ON");
      publishEvent(TOPIC_WAREHOUSEDOOR, "LOCKED");




      for (int i = 0; i < 5; i++) {
        tone(BUZZERB, 1000);
        delay(200);




        noTone(BUZZERB);
        delay(200);
      }




      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);




      publishEvent(TOPIC_LIGHT, "OFF");
      updateLCD("System ACTIVE", "Scan RFID");
      publishEvent(TOPIC_SYSTEM, "NORMAL CONDITION");
      publishEvent(TOPIC_SECURITY, "SYSTEM ACTIVE");
      publishEvent(TOPIC_ALARM, "OFF");
      publishEvent(TOPIC_BUZZERB, "OFF");
      noTone(BUZZERB);
    }




    rfid.PICC_HaltA();
  }




  int currentDoorStatus = digitalRead(REED_SWITCH);




  if (currentDoorStatus != lastDoorStatus) {
    if (currentDoorStatus == HIGH) {
      publishEvent(TOPIC_REEDB, "OPEN");




      if (accessGranted == true) {
        Serial.println("Door Opened Legally");
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, LOW);




        publishEvent(TOPIC_LIGHT, "ON");
        noTone(BUZZERB);
        updateLCD("Door Opened", "Access Valid");
        publishEvent(TOPIC_SECURITY, "SAFE ACCESS");
        publishEvent(TOPIC_BUZZERB, "OFF");
        publishEvent(TOPIC_ALARM, "OFF");
        publishEvent(TOPIC_WAREHOUSEDOOR, "OPENED");
      }




      else {
        Serial.println("WARNING - DOOR FORCED OPEN");
        forcedDoorAlarm = true;
        alarmActive = true;




        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, HIGH);




        publishEvent(TOPIC_LIGHT, "ON");
        updateLCD("WARNING", "Door Forced");
        publishEvent(TOPIC_SECURITY, "DANGER");
        publishEvent(TOPIC_ALARM, "ALARM ACTIVE");
        publishEvent(TOPIC_BUZZERB, "CONTINUOUS");
        tone(BUZZERB, 1000);




        publishEvent(TOPIC_WAREHOUSEDOOR, "LOCKED");
      }
    }




    else {
      publishEvent(TOPIC_REEDB, "CLOSED");
      Serial.println("Door Closed");
      publishEvent(TOPIC_WAREHOUSEDOOR, "LOCKED");




      if (forcedDoorAlarm == true ||
          alarmActive == true) {




        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, HIGH);




        publishEvent(TOPIC_LIGHT, "ON");
        tone(BUZZERB, 1000);




        updateLCD("WARNING", "Door Locked");
        publishEvent(TOPIC_SECURITY, "DANGER");
        publishEvent(TOPIC_ALARM, "ALARM ACTIVE");
        publishEvent(TOPIC_BUZZERB, "CONTINUOUS");
      }




      else if (accessGranted == true) {
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_YELLOW, LOW);
        digitalWrite(LED_RED, LOW);




        publishEvent(TOPIC_LIGHT, "ON");
        noTone(BUZZERB);




        publishEvent(TOPIC_BUZZERB, "OFF");
        publishEvent(TOPIC_ALARM, "OFF");
        updateLCD("Door Closed", "System Secure");
        publishEvent(TOPIC_SECURITY, "SAFE");
        delay(3000);




        updateLCD("System ACTIVE", "Scan RFID");
        publishEvent(TOPIC_SYSTEM, "NORMAL CONDITION");
        publishEvent(TOPIC_SECURITY, "SYSTEM ACTIVE");
       
        accessGranted = false;
      }
    }




    lastDoorStatus = currentDoorStatus;
  }




  if (alarmActive == true ||
      forcedDoorAlarm == true) {




    tone(BUZZERB, 1000);
  }




  delay(100);
}
