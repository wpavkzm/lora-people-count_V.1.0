#include <ArduinoJson.h>

// Libraries for LoRa
#include <SPI.h>
#include <LoRa.h>

// Libraries for OLED Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// define the pins used by the LoRa transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26

// 433E6 for Asia
// 866E6 for Europe
// 915E6 for North America
// 920E6 for South Korea
#define BAND 915E6

// OLED pins
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

int sensorValue = analogRead(A0);
float voltage = sensorValue * (5.0 / 1023.0);

const byte destination_address = 0x00;
const byte local_address = 0x11;

const unsigned long keep_alive_interval = 1000;

// Packet counter
unsigned long data_id = 0;

String payload, data;
const String HEADER_AT_SEND = "at+send=";
unsigned long last_keep_alive_timestamp = 4294967295;
unsigned long last_update_display_timestamp = 4294967295;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

uint8_t refresh_characters_index = 0;
String refresh_characters[] = {"|", "/", "-", "\\"};

void updateDisplay() {
  if (refresh_characters_index > 3) {
    refresh_characters_index = 0;
  }
  String refresh_character = refresh_characters[refresh_characters_index++] + " ";

  unsigned int left_millis = keep_alive_interval - (millis() - last_keep_alive_timestamp);
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(refresh_character + "LORA SENDER " + (String) local_address);
  display.setCursor(0, 10);
  display.print(refresh_character);
  display.setTextColor(BLACK, WHITE);
  display.print((String) (BAND / (1000 * 1000)) + "MHz");
  display.setCursor(0, 20);
  display.setTextColor(WHITE);
//  display.print(refresh_character + WiFi.macAddress());
  display.setCursor(0, 30);
  display.print(refresh_character + "Packet count: " + (String) data_id);

  display.setCursor(0, 40);
  display.print(refresh_character + "KA every " + (String) (keep_alive_interval / 1000) + "s");
  display.setCursor(0, 50);
  display.print(refresh_character + (String) left_millis + "ms");

  display.display();
}

void sendMessage(String payload) {
  LoRa.beginPacket();
  LoRa.write(destination_address);              // add destination address
  LoRa.write(local_address);                   // add sender address
  LoRa.write(data_id++);                  // add message ID
  LoRa.write(payload.length());         // add payload length
  LoRa.print(payload);                  // add payload
  LoRa.endPacket();

  updateDisplay();

  // Output lora payload
  Serial.println(payload);
}

void setup() {
  //reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

//  //initialize OLED
//  Wire.begin(OLED_SDA, OLED_SCL);
//  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
//    Serial.println(F("SSD1306 allocation failed"));
//    for(;;); // Don't proceed, loop forever
//  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("LORA SENDER ");
  display.display();

  //initialize Serial Monitor
  Serial.begin(9600);

  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);

  //setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  if (Serial.available()) {
    String serial_input_str = Serial.readStringUntil('\n');

    if (serial_input_str.startsWith(HEADER_AT_SEND)) {
      String serial_payload = serial_input_str.substring(HEADER_AT_SEND.length());
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, serial_payload);

      // TODO mac_addr 가져오는 코드는 추후 게이트 센서 자체 코드에 병합될 예정
//      doc["bssid"] = WiFi.macAddress();

      // Send LoRa packet to receiver
      String lora_payload;
      serializeJson(doc, lora_payload);
      sendMessage(lora_payload);
    }
    return;
  }

  unsigned long long current_timestamp = millis();
  if (current_timestamp - last_update_display_timestamp >= 100) {
    updateDisplay();
    last_update_display_timestamp = current_timestamp;
  }

  if (current_timestamp - last_keep_alive_timestamp < keep_alive_interval) {
    return;
  }

  // Send test packet
  String serial_input_str = "at+send={}";
  String serial_payload = serial_input_str.substring(HEADER_AT_SEND.length());
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, serial_payload);
  doc["s_type"] = "lora_transmitter";
  doc["d_type"] = "test"; 

  // Send LoRa packet to receiver
  String lora_payload;
  serializeJson(doc, lora_payload);
  sendMessage(lora_payload);
  last_keep_alive_timestamp = current_timestamp;
}
