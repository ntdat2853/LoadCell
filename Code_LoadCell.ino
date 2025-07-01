#include <WiFi.h>
#include <HTTPClient.h>
#include "HX711.h"
#include <Preferences.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>


const char* ssid = "1PHNAD";      
const char* password = "Hongbietmk";  
String gasScriptUrl = "https://script.google.com/macros/s/AKfycbwKCNZVdUqWfXZL6EfJU5hmg7kAKgEILDJeH6XIEnzoV2xw1oe2zEw0EAQG6PzWC3Fk/exec";


#define LOADCELL_DOUT_PIN 5
#define LOADCELL_SCK_PIN 18
#define ROW_NUM 4
#define COLUMN_NUM 3
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'*', '0', '#'}
};
byte pin_rows[ROW_NUM] = {12, 33, 25, 27};
byte pin_column[COLUMN_NUM] = {14, 13, 26};


HX711 LOADCELL_HX711;
Preferences preferences;
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);
LiquidCrystal_I2C lcd(0x27, 16, 2);


struct DataPacket {
  char stt[16];
  float khoiluong;
};
QueueHandle_t dataQueue;



void sendDataToGoogleSheet(String stt, float khoiluong) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Loi ket noi WiFi, huy bo gui.");
    return;
  }

  HTTPClient http;
  String url = gasScriptUrl + "?stt=" + stt + "&khoiluong=" + String(khoiluong, 2);
  Serial.print("Dang gui yeu cau den URL: "); Serial.println(url);
  
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpResponseCode);
  } else {
    Serial.printf("[HTTP] GET... that bai, loi: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}



void taskGuiDuLieu(void *parameter) {
  Serial.println("Task Gui Du Lieu da khoi chay.");
  DataPacket receivedPacket;

  for (;;) {
    if (xQueueReceive(dataQueue, &receivedPacket, portMAX_DELAY) == pdPASS) {
      Serial.printf("Nhan duoc goi tin. Gui: STT %s, KL %.2f\n", receivedPacket.stt, receivedPacket.khoiluong);
      sendDataToGoogleSheet(String(receivedPacket.stt), receivedPacket.khoiluong);
    }
  }
}



void taskNhanDuLieu(void *parameter) {
  Serial.println("Task Nhan Du Lieu da khoi chay.");
  float weight_In_g = 0;
  String inputSTT = "";
  bool isPaused = false;
  bool isNewCycle = true;
  unsigned long lastReadTime = 0;
  const long readInterval = 300;

  for(;;) {
      unsigned long currentTime = millis();
      if (!isPaused && currentTime - lastReadTime >= readInterval) {
        if (LOADCELL_HX711.is_ready()) {
          weight_In_g = LOADCELL_HX711.get_units(10); 
          if (weight_In_g < 1) weight_In_g = 0;
        }
        lastReadTime = currentTime;
      }

      char key = keypad.getKey();
      if (key) {
        isPaused = true;
        if (!isNewCycle) {
          isNewCycle = true; inputSTT = "";
          lcd.clear(); lcd.setCursor(0, 0); lcd.print("STT: ");
          lcd.setCursor(0, 1); lcd.print("KL: ");
        }

        if (key >= '0' && key <= '9') {
          if(inputSTT.length() < 15) {
             inputSTT += key;
          }
          lcd.setCursor(5, 0); lcd.print(inputSTT);
        }
        else if (key == '#') {
          if (inputSTT.length() > 0) {
            inputSTT.remove(inputSTT.length() - 1);
            lcd.setCursor(0, 0); lcd.print("STT:               "); 
            lcd.setCursor(5, 0); lcd.print(inputSTT);
          }
        }
        else if (key == '*') {
          if (inputSTT.length() > 0) {
            DataPacket packetToSend;
            packetToSend.khoiluong = round(weight_In_g);
            inputSTT.toCharArray(packetToSend.stt, 16);

            if (xQueueSend(dataQueue, &packetToSend, (TickType_t) 10) != pdPASS) {
                Serial.println("Loi: Hang doi day.");
            }

            // Hiển thị khối lượng cuối cùng
            lcd.setCursor(0, 1);
            lcd.print("KL:                ");
            lcd.setCursor(4, 1);
            lcd.print(String(packetToSend.khoiluong) + "g");
            vTaskDelay(pdMS_TO_TICKS(300));

            // Reset màn hình ngay lập tức để người dùng nhập tiếp
            isNewCycle = false; isPaused = false; 
            lcd.clear(); lcd.setCursor(0, 0); lcd.print("STT: ");
            lcd.setCursor(0, 1); lcd.print("KL: ");
            inputSTT = "";
          }
        }
      }
      vTaskDelay(pdMS_TO_TICKS(20));
  }
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Ket noi WiFi...");
  
  WiFi.begin(ssid, password);
  int retries = 20;
  while (WiFi.status() != WL_CONNECTED && retries > 0) {
    delay(500); Serial.print(".");
    retries--;
  }

  if (WiFi.status() != WL_CONNECTED){
      lcd.clear(); lcd.print("Loi WiFi!");
      while(1);
  }
  
  Serial.println("\nDa ket noi WiFi!");
  lcd.clear(); lcd.print("Da ket noi!"); delay(1500);

  LOADCELL_HX711.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  preferences.begin("CF", false);
  float LOAD_CALIBRATION_FACTOR = preferences.getFloat("CFVal", 420.0);
  LOADCELL_HX711.set_scale(LOAD_CALIBRATION_FACTOR);
  LOADCELL_HX711.tare(); 

  lcd.clear(); lcd.setCursor(0, 0); lcd.print("STT: ");
  lcd.setCursor(0, 1); lcd.print("KL: ");

  dataQueue = xQueueCreate(10, sizeof(DataPacket));

  if (dataQueue == NULL) {
    Serial.println("Loi tao hang doi!");
    while(1);
  }

  xTaskCreatePinnedToCore(
    taskNhanDuLieu, "NhanDuLieu",
    4096, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(
    taskGuiDuLieu, "GuiDuLieu",
    8192, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}