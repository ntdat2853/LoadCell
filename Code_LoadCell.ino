#include <WiFi.h>
#include <HTTPClient.h>
#include "HX711.h"
#include <Preferences.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

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
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);
LiquidCrystal_I2C lcd(0x27, 16, 2);

struct DataPacket {
  char stt[16];
  float khoiluong;
};
QueueHandle_t dataQueue;

const char* ap_ssid = "Thiet lap ESP32"; 
const char* ap_password = "0867688330";
const char* mDNS_name = "can-thong-minh";

WebServer server(80);
DNSServer dnsServer;
Preferences preferences; 

String sta_ssid = "";
String sta_password = "";

bool isConfigMode = false;

String getCSS() {
  return "<style>"
         "body{font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; margin: 0; padding: 20px;}"
         ".container{max-width: 600px; margin: 0 auto; background: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1);}"
         "h1,h2{text-align: center; color: #0056b3;}"
         "form{display: flex; flex-direction: column;}"
         "label{margin-bottom: 5px; font-weight: bold;}"
         "input[type='text'], input[type='password']{padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 4px;}"
         "input[type='submit']{padding: 12px; border: none; background-color: #0056b3; color: white; font-size: 16px; border-radius: 4px; cursor: pointer;}"
         "input[type='submit']:hover{background-color: #004494;}"
         ".wifi-list a{display: block; padding: 10px; margin-bottom: 5px; text-decoration: none; color: #0056b3; border-radius: 4px; border: 1px solid #0056b3; text-align: center;}"
         ".wifi-list a:hover{background-color: #f0f8ff;}"
         "</style>";
}

void handleScan() {
  String html = "<!DOCTYPE html><html lang='vi'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Quét WiFi</title>";
  html += getCSS();
  html += "</head><body><div class='container'>";
  html += "<h1>Chọn Mạng WiFi</h1>";
  html += "<div class='wifi-list'>";
  
  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<p>Không tìm thấy mạng WiFi nào.</p>";
  } else {
    for (int i = 0; i < n; ++i) {
      String ssid_name = WiFi.SSID(i);
      html += "<a href='#' onclick='document.getElementById(\"ssid\").value = \"" + ssid_name + "\"; document.getElementById(\"password\").focus(); return false;'>" + ssid_name + " (" + WiFi.RSSI(i) + " dBm)</a>";
    }
  }
  html += "</div>";
  html += "<h2>Nhập thông tin mạng đã chọn</h2>";
  html += "<form action='/save' method='POST'>";
  html += "<label for='ssid'>Tên WiFi (SSID):</label>";
  html += "<input type='text' id='ssid' name='ssid' required>";
  html += "<label for='password'>Mật khẩu:</label>";
  html += "<input type='password' id='password' name='password'>";
  html += "<input type='submit' value='Lưu và Khởi động lại'>";
  html += "</form>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  sta_ssid = server.arg("ssid");
  sta_password = server.arg("password");
  
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", sta_ssid);
  preferences.putString("password", sta_password);
  preferences.end();
  
  String html = "<html><body><h1>Đã lưu. Đang khởi động lại...</h1></body></html>";
  server.send(200, "text/html", html);
  
  delay(2000);
  ESP.restart();
}

void handleRoot_ConfigMode() {
    handleScan();
}

void sendDataToGoogleSheet(String stt, float khoiluong) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Loi ket noi WiFi, huy bo gui.");
    lcd.clear();
    lcd.print("Loi WiFi!");
    delay(2000);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("STT: ");
    lcd.setCursor(0, 1); lcd.print("KL: ");
    return;
  }

  HTTPClient http;
  String url = gasScriptUrl + "?stt=" + stt + "&khoiluong=" + String(khoiluong, 2);
  Serial.print("Dang gui yeu cau den URL: "); Serial.println(url);
  
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

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
            lcd.setCursor(0, 0); lcd.print("STT:              "); 
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
            
            lcd.setCursor(0, 1);
            lcd.print("KL:               ");
            lcd.setCursor(4, 1);
            lcd.print(String(packetToSend.khoiluong) + "g");
            vTaskDelay(pdMS_TO_TICKS(300));

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

void setupScaleApplication() {
  LOADCELL_HX711.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  preferences.begin("CF", false); 
  float LOAD_CALIBRATION_FACTOR = preferences.getFloat("CFVal", 420.0);
  preferences.end();
  
  LOADCELL_HX711.set_scale(LOAD_CALIBRATION_FACTOR);
  LOADCELL_HX711.tare(); 

  lcd.clear(); lcd.setCursor(0, 0); lcd.print("STT: ");
  lcd.setCursor(0, 1); lcd.print("KL: ");

  dataQueue = xQueueCreate(10, sizeof(DataPacket));
  if (dataQueue == NULL) {
    Serial.println("Loi tao hang doi!");
    while(1);
  }

  xTaskCreatePinnedToCore(taskNhanDuLieu, "NhanDuLieu", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskGuiDuLieu, "GuiDuLieu", 8192, NULL, 1, NULL, 1);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dang khoi dong...");

  preferences.begin("wifi-config", true); 
  sta_ssid = preferences.getString("ssid", "");
  sta_password = preferences.getString("password", "");
  preferences.end();

  bool connected = false;
  if (sta_ssid.length() > 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ket noi WiFi...");
    Serial.println("Da co cau hinh WiFi, dang thu ket noi...");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
    
    int retries = 30; 
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
      delay(500);
      Serial.print(".");
      lcd.print(".");
      retries--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
    }
  }

  if (connected) {
    isConfigMode = false;
    Serial.println("\nDa ket noi WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.print("Da ket noi!");
    delay(1500);
    
    setupScaleApplication();

  } else {
    isConfigMode = true;
    Serial.println("\nKhong co cau hinh hoac ket noi that bai. Chuyen sang che do cai dat.");
    lcd.clear();
    lcd.print("Che do Cai dat");
    delay(2000);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ap_ssid, ap_password);
    
    IPAddress apIP = WiFi.softAPIP();
    
    Serial.print("Access Point '");
    Serial.print(ap_ssid);
    Serial.println("' da bat dau.");
    Serial.print("IP cua AP: ");
    Serial.println(apIP);

    lcd.clear();
    lcd.print("Ten WiFi:");
    lcd.setCursor(0, 1);
    lcd.print(ap_ssid);

    dnsServer.start(53, "*", apIP);

    server.on("/", HTTP_GET, handleRoot_ConfigMode);
    server.on("/scan", HTTP_GET, handleScan);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot_ConfigMode);
    server.begin();
  }
}

void loop() {
  if (isConfigMode) {
    dnsServer.processNextRequest();
    server.handleClient();
  } else {
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}