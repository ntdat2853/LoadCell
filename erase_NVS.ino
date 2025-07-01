#include <nvs_flash.h>

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(1000);

  nvs_flash_erase(); 
  nvs_flash_init();  
  Serial.println("Xong");
}

void loop() {
  delay(1000);
}