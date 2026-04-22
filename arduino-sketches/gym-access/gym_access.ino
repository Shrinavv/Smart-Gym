#include <SPI.h>
#include <MFRC522.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <SoftwareSerial.h>

SoftwareSerial Serial1(7, 6); // RX, TX for ESP8266
MFRC522 mfrc522(10, 9);       // SS, RST

char ssid[] = "YOUR_WIFI_SSID";      // ← CHANGE
char pass[] = "YOUR_WIFI_PASSWORD";  // ← CHANGE
char writeAPIKey[] = "YOUR_GYM_WRITE_API_KEY"; // ← CHANGE
unsigned long channelID = YOUR_GYM_CHANNEL_ID; // ← CHANGE

WiFiEspClient client;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  WiFi.init(&Serial1);
  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("Connecting to WiFi...");
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.println("Gym Access Ready - Scan card to log IN/OUT");
}

unsigned long getCardUID() {
  unsigned long uid = 0;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid = (uid << 8) | mfrc522.uid.uidByte[i];
  }
  return uid;
}

void sendToThingSpeak(unsigned long userID) {
  if (client.connect("api.thingspeak.com", 80)) {
    String url = "/update?api_key=" + String(writeAPIKey) +
                 "&field1=" + String(userID);
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.thingspeak.com\r\n" +
                 "Connection: close\r\n\r\n");
    client.stop();
    Serial.println("Logged access for UID: " + String(userID));
  }
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  unsigned long uid = getCardUID();
  sendToThingSpeak(uid);

  // Visual feedback
  Serial.println("Card scanned! Access logged (IN/OUT paired in dashboard).");
  delay(1000); // prevent multiple scans
  mfrc522.PICC_HaltA();
}