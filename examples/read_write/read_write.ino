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
  #if (defined STM32F4xx) || (defined STM32F1xx)
  W25QX.begin(PB7);
  #elif defined(ESP32)
  W25QX.begin(25);
  #endif
  Serial.println(W25QX.read_id(),HEX);
  Serial.println(W25QX.read_chipid(),HEX);
  uint8_t readbuf[15] = "";
  uint8_t sendbuf[15] = "hello jack";
  W25QX.write_arbitrary_data(1000,sendbuf,10);
  W25QX.read_data(1000,readbuf,10);
  Serial.println((const char *)readbuf);
}
void loop(){

}