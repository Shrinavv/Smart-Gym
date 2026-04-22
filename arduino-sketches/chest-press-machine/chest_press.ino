#include <SPI.h>
#include <MFRC522.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <SoftwareSerial.h>

SoftwareSerial Serial1(7, 6);
MFRC522 mfrc522(10, 9);

char ssid[] = "YOUR_WIFI_SSID";           // ← CHANGE
char pass[] = "YOUR_WIFI_PASSWORD";       // ← CHANGE
char writeAPIKey[] = "YOUR_MACHINE_WRITE_API_KEY"; // ← CHANGE
unsigned long channelID = YOUR_MACHINE_CHANNEL_ID; // ← CHANGE

WiFiEspClient client;

const int IR_PIN = 8;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long MIN_REP_INTERVAL = 1500; // 1.5 seconds

unsigned long lastDebounceTime = 0;
int lastReading = LOW;
int debouncedState = LOW;

bool sessionActive = false;
unsigned long sessionStartMillis = 0;
unsigned long userIDInSession = 0;
int repCount = 0;
bool isAtPeak = false;
unsigned long lastRepTime = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  WiFi.init(&Serial1);
  SPI.begin();
  mfrc522.PCD_Init();
  pinMode(IR_PIN, INPUT);

  Serial.println("Connecting to WiFi...");
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.println("Chest Press Smart Machine Ready");
  Serial.println("Scan RFID to START session");
}

unsigned long getCardUID() { /* same as above */ 
  unsigned long uid = 0;
  for (byte i = 0; i < mfrc522.uid.size; i++) uid = (uid << 8) | mfrc522.uid.uidByte[i];
  return uid;
}

void sendToThingSpeak(unsigned long uid, int reps, unsigned long durationSec, bool isStart) {
  if (client.connect("api.thingspeak.com", 80)) {
    String url = "/update?api_key=" + String(writeAPIKey) +
                 "&field1=" + String(uid) +
                 "&field2=" + String(isStart ? 0 : reps) +
                 "&field3=" + String(isStart ? 0 : durationSec);
    client.print(String("GET ") + url + " HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n");
    client.stop();
  }
}

void loop() {
  // ---------- RFID ----------
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    unsigned long uid = getCardUID();

    if (!sessionActive) {
      // START session
      sessionActive = true;
      sessionStartMillis = millis();
      userIDInSession = uid;
      repCount = 0;
      isAtPeak = false;
      lastRepTime = 0;
      sendToThingSpeak(uid, 0, 0, true); // start flag
      Serial.println("SESSION STARTED for UID: " + String(uid));
    } else if (uid == userIDInSession) {
      // END session
      unsigned long duration = (millis() - sessionStartMillis) / 1000;
      sendToThingSpeak(uid, repCount, duration, false);
      Serial.println("SESSION ENDED - Reps: " + String(repCount) + " | Duration: " + String(duration) + "s");
      sessionActive = false;
    }
    delay(1500);
    mfrc522.PICC_HaltA();
  }

  // ---------- IR REP COUNTING (only when session active) ----------
  if (!sessionActive) return;

  int reading = digitalRead(IR_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != debouncedState) {
      debouncedState = reading;

      // State machine (matches your 4-stage logic + noise rules)
      if (debouncedState == HIGH && !isAtPeak) {
        isAtPeak = true;           // pushing → peak
      }

      if (debouncedState == LOW && isAtPeak) {
        if (millis() - lastRepTime > MIN_REP_INTERVAL) {
          repCount++;
          lastRepTime = millis();
          Serial.print("REP #"); Serial.println(repCount);
          isAtPeak = false;        // returning → count + idle
        }
      }
    }
  }
  lastReading = reading;
}