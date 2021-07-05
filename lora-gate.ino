//#include <SoftwareSerial.h>
#include "U8glib.h"
#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF95.h>

#define DIO1 2;
#define RFM95_INT 3
#define RFM95_RST 9
#define RFM95_CS 10
#define MOSI 11;
#define MISO 12;
#define SCK 13;

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

//SoftwareSerial LoRaSerial(2, 3); // Rx, Tx

//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);


int LED = A4;
int led = A5;
uint8_t IR1 = 5; // IR sensor 1
uint8_t IR2 = 6; // IR sensor 2
uint8_t PIR = 7; // PIR sensor 
uint8_t beep_pin = 8;
uint8_t sensor1 = HIGH;
uint8_t sensor2 = HIGH;
int t;
unsigned int wt = 2000; //wait time //1000이 더 정확한거 같음  
int waited_time = 0; // time_threashold to break while_loop

uint16_t eeprom_address_data_id_flag = 0x00;
uint16_t eeprom_address_data_id_value = 0x01;
unsigned long data_id = 0;                // LoRa로 전송되는 데이터의 고유번호
int8_t people_count = 0;                 // LoRa 모듈을 통해 전송되기 전까지 저장(전송 후에는 0으로 리셋)
//int8_t people_count_for_display = 0;     // 누적되는 데이터

// Keep alive
bool enable_keep_alive = true;
unsigned int keep_alive_interval = 300;
unsigned long last_keep_alive_timestamp = 4294967295;

void save_data_id() {
    EEPROM.put(eeprom_address_data_id_value, data_id);
}

void load_data_id() {
    if (EEPROM.read(eeprom_address_data_id_flag) != 111) {
        // It's meaning not initialized
        EEPROM.put(eeprom_address_data_id_value, data_id);
        EEPROM.write(eeprom_address_data_id_flag, 111);
    }
    EEPROM.get(eeprom_address_data_id_value, data_id);
}

// TODO 기기의 GPIO에 LoRa 모듈이 직접 연결되면
// 시리얼 통신으로 명령을 내리는 것이 아닌
// 직접 LoRa 모듈으로 데이터를 전송하도록 한다.

/////////////////////////////////////////////////////////////////////////////
void send_data(String data){
  if (rf95.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.recv(buf, &len))
    {
      RH_RF95::printBuffer("Received: ", buf, len);
      //Serial.print("Got: ");
      //Serial.println((char*)buf);
     //  Serial.print("RSSI: ");
      //Serial.println(rf95.lastRssi(), DEC);
      
      // Send a reply
//      uint8_t data[] = "And hello back to you";
//      rf95.send(data, sizeof(data));
//      rf95.waitPacketSent();
     // Serial.println("Sent a reply");
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
}

////////////////////////////////////////////////////////////////

//void send_data(String data) {
//    Serial.println(data);
//    LoRaSerial.println("at+send=" + data);
//    
//}

void send_data_keep_alive() {
    int sensorValue = analogRead(A0);
    float voltage = (sensorValue * (5.0 / 1023.0)) * 1000;
    send_data("{\"s_type\":\"gate\",\"d_type\":\"keep_alive\",\"vcc\":" + (String) round(voltage) + "}");
}

void send_data_people_count() {
    send_data("{\"s_type\":\"gate\",\"d_type\":\"inout_evt\",\"d_id\":" + ((String) data_id++) + ",\"cnt\":" + (String) people_count + "}");
    save_data_id();
    people_count = 0;
}

int ReadSensors() {
    int s1_high = 0;
    int s1_low = 0;
    int s2_high = 0;
    int s2_low = 0;
    int count = 70;
    while ((s1_high <= count && s1_low <= count) || (s2_high <= count && s2_low <= count)) {
        //    delay(100);
        //    Serial.print(s1_high);
        //    Serial.print(", ");
        //    Serial.print(s1_low);
        //    Serial.print(", ");
        //    Serial.print(s2_high);
        //    Serial.print(", ");
        //    Serial.println(s2_low);
        sensor1 = digitalRead(IR1);
        sensor2 = digitalRead(IR2);
        //    Checksensors(sensor1, sensor2);
        if (sensor1 == HIGH) {
            ++s1_high;
            s1_low = 0;
        } else {
            ++s1_low;
            s1_high = 0;
        }
        if (sensor2 == HIGH) {
            ++s2_high;
            s2_low = 0;
        } else {
            ++s2_low;
            s2_high = 0;
        }
    }
    if (s1_high > s1_low) {
        sensor1 = HIGH;
    } else {
        sensor1 = LOW;
    }
    if (s2_high > s2_low) {
        sensor2 = HIGH;
    } else {
        sensor2 = LOW;
    }
    //  Serial.println("I will return!");
    return sensor1, sensor2;
}

//void Display(int count) {
//    u8g.firstPage();
//    do {
//        u8g.setFont(u8g_font_osb18);
//        u8g.setPrintPos(10, 20);
//        u8g.print("ZeroWeb!");
//        u8g.drawStr(10, 45, "Count:");
//        u8g.setPrintPos(90, 45);
//        u8g.print(count);
//    } while (u8g.nextPage());
//}

void Beep() {
    tone(beep_pin, 1000, 500);
    delay(70);
    noTone(beep_pin);
}

void setup() {
///////////////////////////////////////////////////////////////////////////////////////
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // Serial communication
    Serial.begin(9600);
//    LoRaSerial.begin(9600);

    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
    }
    Serial.println("LoRa radio init OK!");
  
    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
    if (!rf95.setFrequency(RF95_FREQ)) {
      Serial.println("setFrequency failed");
      while (1);
    }
    Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

    rf95.setTxPower(23, false);

////////////////////////////////////////////////////////////////////////////////////////////

    // IR Sensor
    pinMode(IR1, INPUT);
    pinMode(IR2, INPUT);

    load_data_id();

    delay(100);
    send_data_keep_alive();

}

void loop() {
 
    sensor1, sensor2 = ReadSensors();

    if (sensor1 == HIGH && sensor2 == LOW) { // (1)
        //    Serial.println("Im in if");
        t = millis();
        sensor1, sensor2 = ReadSensors();
        while (sensor2 == LOW || sensor1 == LOW) { //Wait until sensor2 becomes HIGH
            sensor1, sensor2 = ReadSensors();
            waited_time = millis() - t;
            if (waited_time > wt) {
                break;
            }
        }
        if (sensor1 == HIGH && sensor2 == HIGH) { // (2)
            t = millis();
            sensor1, sensor2 = ReadSensors();
            while (sensor1 == HIGH || sensor2 == LOW) { //Wait until sensor1 becomes LOW
                sensor1, sensor2 = ReadSensors();
                waited_time = millis() - t;
                if (waited_time > wt) {
                    break;
                }
            }
            if (sensor1 == LOW && sensor2 == HIGH) { // (3)
                t = millis();
                sensor1, sensor2 = ReadSensors();
                while (sensor1 == HIGH || sensor2 == HIGH) { //Wait until sensor2 becomes LOW
                    sensor1, sensor2 = ReadSensors();
                    waited_time = millis() - t;
                    if (waited_time > wt) {
                        break;
                    }
                }
                if (sensor1 == LOW && sensor2 == LOW) { // 4
                    ++people_count;
                    //++people_count_for_display;
                    //          Serial.println("IN:");
                    //          Serial.println(++people_count);
                    Serial.println("IN: " + (String) people_count);
                    Beep();
                    //Display(people_count_for_display);
                    send_data_people_count();
                }
            }
        }
    } else if (sensor1 == LOW && sensor2 == HIGH) { // (1)
        t = millis();
        sensor1, sensor2 = ReadSensors();
        while (sensor1 == LOW || sensor2 == LOW) { //Wait until sensor1 becomes HIGH
            sensor1, sensor2 = ReadSensors();
            waited_time = millis() - t;
            if (waited_time > wt) {
                break;
            }
        }
        if (sensor1 == HIGH && sensor2 == HIGH) { // (2)
            t = millis();
            sensor1, sensor2 = ReadSensors();
            while (sensor1 == LOW || sensor2 == HIGH) { //Wait until sensor2 becomes LOW

               sensor1, sensor2 = ReadSensors();
                waited_time = millis() - t;
                if (waited_time > wt) {
                    break;
                }
            }
            if (sensor1 == HIGH && sensor2 == LOW) { // (3)
                t = millis();
                sensor1, sensor2 = ReadSensors();
                while (sensor1 == HIGH || sensor2 == HIGH) { //Wait until sensor1 becomes LOW
                    sensor1, sensor2 = ReadSensors();
                    waited_time = millis() - t;
                    if (waited_time > wt) {
                        break;
                    }
                }
                if (sensor1 == LOW && sensor2 == LOW) { // 4
                    --people_count;
                   // --people_count_for_display;

                    Serial.println("OUT: " + (String) people_count);
                    //          if (--people_count < 0) {
                    //            IN = 0;
                    //          }
                    Beep();
                    //Display(people_count_for_display);
                    send_data_people_count();
                }
            }
        }
    }

    // Keep alive handler
    if (enable_keep_alive) {
        unsigned long current_timestamp = millis();
        if (current_timestamp - last_keep_alive_timestamp < keep_alive_interval) {
            return;
        }
        send_data_keep_alive();
        last_keep_alive_timestamp = current_timestamp;
    }
}

void Checksensors(int sensor1, int sensor2) {
    if (sensor1 == HIGH) {
        Serial.println("sensor1");
    }
    if (sensor2 == HIGH) {
        Serial.println("sensor2");
    }
    //  Serial.print("sensor1: ");
    //  Serial.println(sensor1);
    //  Serial.print("sensor2: ");
    //  Serial.println(sensor2);
    //  Serial.println("-------------------------");
}
