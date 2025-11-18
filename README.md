# SFLib

You can easily read and write flash storage, such as W25Q32

This lib is available for ESP32\ESP8266 STM32dunio Arduino



example
```cpp
#include <Arduino.h>
#include <SPI.h>
#include "SFLib.h"

SFLib W25QX;

void setup() {
  #if (defined STM32F4xx) || (defined STM32F1xx) 
  Serial.setRx(PA10);
  Serial.setTx(PA9);
  #endif
  Serial.begin(115200);
  
  W25QX.begin(); 
  Serial.println(W25QX.read_id());
  Serial.println(W25QX.read_chipid());
  uint8_t readbuf[10] = "";
  uint8_t sendbuf[10] = "haha123";
  W25QX.write_arbitrary_data(1000,sendbuf,7);
  W25QX.read_data(1000,readbuf,7);
  Serial.println((const char *)readbuf);
}
void loop(){

}
```