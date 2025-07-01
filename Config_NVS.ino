#include "HX711.h"
#include <Preferences.h>

#define LOADCELL_DOUT_PIN 5
#define LOADCELL_SCK_PIN  18

#define KL0 500                                                                                   

long sensor_Reading_Results; 
float CALIBRATION_FACTOR;
bool show_Weighing_Results = false;

int weight_In_g;    

HX711 LOADCELL_HX711;
Preferences preferences;

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(2000);

  Serial.println("Setup...");
  delay(1000);

  preferences.begin("CF", false);
  delay(100);

  Serial.println();
  Serial.println("Khong dat vat len can");
  delay(1000);

  Serial.println();
  Serial.println("LOADCELL_HX711 begin.");
  LOADCELL_HX711.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  Serial.println();
  Serial.println("Setup finish.");
  delay(1000);

  Serial.println();
  Serial.println("Gui yeu cau :");
  Serial.println("1. P de chuan bi hieu chinh");
  Serial.println("2. C de tien hanh hieu chinh");
  delay(1000);
}

void loop() {
  if(Serial.available()) {
    char inChar = (char)Serial.read(); 
    Serial.println();
    Serial.print("Nhan Ky tu : ");
    Serial.println(inChar);

    if (inChar == 'P') {
      show_Weighing_Results = false;
      delay(1000);
      if (LOADCELL_HX711.is_ready()) {  
        Serial.println(); 
        Serial.println("Hay de can trong");
        Serial.println("...");
        
        for (byte i = 5; i > 0; i--) {
          Serial.println(i);
          delay(1000);
        }
        
        LOADCELL_HX711.set_scale(); 
        Serial.println();
        Serial.println("Dat can");
        Serial.println("...");
        delay(1000);
        
        LOADCELL_HX711.tare();
        Serial.println();
        Serial.println("Hay dat vat len can");
        Serial.flush();
        
        for (byte i = 5; i > 0; i--) {
          Serial.println(i);
          delay(1000);
        }
        
        Serial.println();
        Serial.println("Tiep do hay an C");
      } else {
        Serial.println("HX711 not found.");
      }
    }

    // Conditions to start the calibration process.
    if (inChar == 'C') {
      if (LOADCELL_HX711.is_ready()) {
        Serial.println(); 
        Serial.println("Bat dau hieu chinh");
        Serial.println(); 
        
        for (byte i = 0; i < 5; i++) {
          sensor_Reading_Results = LOADCELL_HX711.get_units(10);
          Serial.print("KQ: ");
          Serial.println(sensor_Reading_Results);
          delay(1000);
        }

        CALIBRATION_FACTOR = sensor_Reading_Results / KL0; 

        Serial.println(); 
        Serial.println("Luu vao flash memory.");
        preferences.putFloat("CFVal", CALIBRATION_FACTOR); 
        delay(500);

        Serial.println(); 
        Serial.println("Thong so hieu chinh tu flash memory.");
        float LOAD_CALIBRATION_FACTOR; 
        LOAD_CALIBRATION_FACTOR = preferences.getFloat("CFVal", 0); 
        delay(500);

        Serial.println(); 
        Serial.println("Cai dat can voi thong so hieu chinh");
        LOADCELL_HX711.set_scale(LOAD_CALIBRATION_FACTOR);
        delay(500);
        
        Serial.println(); 
        Serial.print("CALIBRATION_FACTOR : ");
        Serial.println(LOAD_CALIBRATION_FACTOR);
        delay(2000);
        
        show_Weighing_Results = true;

        Serial.println();
        Serial.println("Xong");
        Serial.println();
        Serial.println("Hieu chuan lai thi gui P");
        Serial.println();
        Serial.println("San sang su dung");
        Serial.println();
        delay(2000);
      } else {
        Serial.println("HX711 not found.");
      }
    }
  }
  
  if (show_Weighing_Results == true) {
    if (LOADCELL_HX711.is_ready()) {
      weight_In_g = LOADCELL_HX711.get_units(10); 

      Serial.print("Khoi luong : ");
      Serial.print(weight_In_g);
      Serial.print(" g");
    } else {
      Serial.println("HX711 not found.");
    }
  }
  delay(1000);
}

