#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#define SS_PIN 2   // SCK - D4
#define RST_PIN 0 // RST - D3

#define BUZZER_RELAY_PIN 4 // D2
#define RELAY_PIN 10 // SD3
#define SENSOR_PIN 5 // D1

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

String strID;
String serverName = "http://192.168.1.17:5000/rfid/validate";

const char *ssid = "ZTE";
const char *password = "999999999";
int wifiStatus;

void connectWifi();
bool readRFID();
bool validateRFID(String strID);

void setup() {
  Serial.begin(9600);
  connectWifi();

  // Init RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("\nI am waiting for card...");

  // init buzzer & relay
  pinMode(BUZZER_RELAY_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // init sensor
  pinMode(SENSOR_PIN, INPUT);
}

void loop() {
  digitalWrite(BUZZER_RELAY_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()){
    if (readRFID()){
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("\nCard valid!");
      delay(7000);
    } else{
      digitalWrite(BUZZER_RELAY_PIN, HIGH);
      Serial.println("\nCard invalid!");
      delay(500);
    }
  }
  if(digitalRead(SENSOR_PIN) == HIGH){
    Serial.println("Motion detected!");
    digitalWrite(BUZZER_RELAY_PIN, HIGH);
    delay(2000);
  }

}

void connectWifi(){
   // wifi connection
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  wifiStatus = WiFi.status();
  if(wifiStatus == WL_CONNECTED){
    Serial.println("");
    Serial.println("ESP8266 sudah terkonesi dg Wifi!");  
    Serial.println("IP address esp8266 : ");
    Serial.println(WiFi.localIP());  
  } else{
    Serial.println("");
    Serial.println("WiFi tdk terkoneksi");
  }
}

bool readRFID(){
  // Serial.print(F("PICC type\t: "));
  // MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  // Serial.println(rfid.PICC_GetTypeName(piccType));

  //id kartu dan yang akan dikirim ke database
  strID = "";
  for (byte i = 0; i <rfid.uid.size; i++) {
    strID +=
      (rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
      String(rfid.uid.uidByte[i], HEX) +
      (i != rfid.uid.size-1 ? ":" : "");
  }

  strID.toUpperCase();
  // Serial.print("Kartu ID Anda\t: ");
  // Serial.println(strID);
  return validateRFID(strID);
}

bool validateRFID(String strID){
  WiFiClient client;
  HTTPClient http;

  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  doc["id"] = (String) strID;
  String json;
  serializeJsonPretty(doc, json);
  Serial.println(json);

  http.begin(client, serverName);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6IjYzMWMxMGM3NTA5MjBmNTg2ZDA1M2RhOCIsInVzZXJuYW1lIjoiUmFuZHkiLCJwYXNzd29yZCI6IiQyYiQxMiRydkQ0cnZrejFkRDJtdUxCV2RxUXhPRkZKV0xXRmxTbXVZdWI5WUw2RjNFL01uOHFIeUlodSIsImlhdCI6MTY2Mjc4OTAxMX0.3B1gXpzdEqsWwOs9Y6jzTRyHw4OmSYHF4Ry359Ex4Ww");

  int httpResponseCode = http.POST(json);
  Serial.println(httpResponseCode);
  Serial.println(http.getString());
  
  if (httpResponseCode == HTTP_CODE_OK) {
    return true;
  } else {
    return false;
  }
}