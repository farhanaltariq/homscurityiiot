#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#define SS_PIN 2   // SCK - D4
#define RST_PIN 0 // RST - D3

#define BUZZER_RELAY_PIN 4 // D2
const int RELAY_PIN = D0;
#define SENSOR_PIN 5 // D1

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

String strID;
String host = "homscurity.up.railway.app";

const char *ssid = "Punyaku";
const char *password = "acyc3937";
int wifiStatus;
float voltage;
int bat_percentage;
int analogInPin  = A0;    // Analog input pin
int sensorValue;
float calibration = 0.72; // Check Battery voltage using multimeter & add/subtract the value

void connectWifi();
bool readRFID();
bool validateRFID(String strID);
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max);
void updateBatPercentage();

unsigned long previousTime = 0;
const unsigned long interval = 10000; // interval waktu dalam milidetik (1 detik)

void setup() {
  Serial.begin(9600);

  // Init RFID
  SPI.begin();
  rfid.PCD_Init();

  // init buzzer & relay
  pinMode(BUZZER_RELAY_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  // init sensor
  pinMode(SENSOR_PIN, INPUT);
}

void loop() {
  connectWifi();

  unsigned long currentTime = millis();

  // Periksa apakah sudah melewati interval waktu
  if (currentTime - previousTime >= interval) {
    previousTime = currentTime; // Simpan waktu saat ini

    // Panggil fungsi tugas secara asinkronus
    updateBatPercentage();
  }


  digitalWrite(BUZZER_RELAY_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (readRFID() == true){
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("\nCard valid!");
      delay(7000);
      digitalWrite(RELAY_PIN, HIGH);
    } else{
      digitalWrite(BUZZER_RELAY_PIN, HIGH);
      Serial.println("\nCard invalid!");
      delay(500);
    }
  } else if(digitalRead(SENSOR_PIN) == HIGH){
    Serial.println("Motion detected!");
    digitalWrite(BUZZER_RELAY_PIN, HIGH);
    delay(2000);
  }

}

void connectWifi(){
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

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

void updateBatPercentage(){
  sensorValue = analogRead(analogInPin);
  voltage = (((sensorValue * 3.3) / 1024) * 2 + calibration); //multiply by two as voltage divider network is 100K & 100K Resisto
  bat_percentage = mapfloat(voltage, 3.0, 4.1, 0, 100); //3.0V as Battery Cut off Voltage & 4.1V as Maximum Voltage
  
  if (bat_percentage >= 100)
  {
    bat_percentage = 100;
  }
  if (bat_percentage <= 0)
  {
    bat_percentage = 1;
  }
  Serial.println(voltage);
  Serial.println(String(bat_percentage) + "%");

   WiFiClientSecure client;

  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  doc["battery"] =  bat_percentage;
  String json;
  serializeJsonPretty(doc, json);
  // Serial.println(json);

  // Use WiFiClient class to create TCP connections
  const int httpsPort = 443; // 443 is for HTTPS!
  
  client.setInsecure(); // this is the magical line that makes everything work
  const String endpoint = "/battery";

  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  // Send the HTTP POST request
  client.print("POST " + endpoint + " HTTP/1.1\r\n");
  client.print("Host: " + host + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6IjY0M2QwNDg1ODg5ZTM3NGFiMzliMDUzMCIsInVzZXJuYW1lIjoiUmFuZHkiLCJwYXNzd29yZCI6IiQyYiQxMiRVb2EvRDNtOHQza0dmWFRMWW03d3FlSVluYW1GY05nRHphWVRwQ3J1R2NqekJPaTNKOXYuaSIsImlhdCI6MTY4NTA2NTc3NH0.e_7GI4vvvcrtf5HwXkPINcE9ld3hoTXCKACwvk-xNyM\r\n");
  client.print("Content-Length: " + String(json.length()) + "\r\n");
  client.print("\r\n");
  client.print(json);

  // Wait for server's response
  while (!client.available()) {
    delay(10);
  }

// Read the response status line
  String responseStatus = client.readStringUntil('\r');
  Serial.print("Response status: ");
  Serial.println(responseStatus);

  rfid.PCD_Init();
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
  Serial.print("Kartu ID Anda\t: ");
  Serial.println(strID);
  return validateRFID(strID);
}

bool validateRFID(String strID){
  WiFiClientSecure client;

  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  doc["id"] = (String) strID;
  String json;
  serializeJsonPretty(doc, json);
  // Serial.println(json);

  // Use WiFiClient class to create TCP connections
  const int httpsPort = 443; // 443 is for HTTPS!
  
  client.setInsecure(); // this is the magical line that makes everything work
  const String endpoint = "/rfid/validate";

  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return false;
  }

  // Send the HTTP POST request
  client.print("POST " + endpoint + " HTTP/1.1\r\n");
  client.print("Host: " + host + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6IjY0M2QwNDg1ODg5ZTM3NGFiMzliMDUzMCIsInVzZXJuYW1lIjoiUmFuZHkiLCJwYXNzd29yZCI6IiQyYiQxMiRVb2EvRDNtOHQza0dmWFRMWW03d3FlSVluYW1GY05nRHphWVRwQ3J1R2NqekJPaTNKOXYuaSIsImlhdCI6MTY4NTA2NTc3NH0.e_7GI4vvvcrtf5HwXkPINcE9ld3hoTXCKACwvk-xNyM\r\n");
  client.print("Content-Length: " + String(json.length()) + "\r\n");
  client.print("\r\n");
  client.print(json);

  // Wait for server's response
  while (!client.available()) {
    delay(10);
  }

  // Read the response status line
  String responseStatus = client.readStringUntil('\r');
  Serial.print("Response status: ");
  Serial.println(responseStatus);

  // Extract the response code from the status line
  int responseCode = responseStatus.substring(9, 12).toInt();
  Serial.print("HTTP Response code: ");
  Serial.println(responseCode);

  Serial.println("closing connection");
  if (responseCode == 200 || responseCode == 201) {
    return true;
  } else {
    return false;
  }
}


float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}