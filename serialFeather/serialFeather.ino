#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <UbloxGPS.h>
 
Uart Serial2 (&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);
UbloxGPS ublox = UbloxGPS(&Serial2); 

boolean fix = false;

void SERCOM1_Handler()
{
  Serial2.IrqHandler();
}
 
void setup() {
  Serial.begin(9600);
 
  Serial2.begin(9600);
  
  // Assign pins 10 & 11 SERCOM functionality
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);

  ublox.init();
}
 
uint8_t i=0;
void loop() {
  //if(Serial2.available()>0) {Serial.write(Serial2.read());}
  ublox.update();

  //log data once every second
  if(millis()%1500 == 0) {
    //All data is returned as numbers (int or float as appropriate), so values must be converted to strings before logging
    double alt = ublox.getAlt_feet();
    String data = String(ublox.getMonth()) + "/" + String(ublox.getDay()) + "/" + String(ublox.getYear()) + ","
                  + String(ublox.getHour()-5) + ":" + String(ublox.getMinute()) + ":" + String(ublox.getSecond()) + ","
                  + String(ublox.getLat()) + "," + String(ublox.getLon()) + "," + String(alt) + ","
                  + String(ublox.getSats()) + ",";
    //GPS should update once per second, if data is more than 2 seconds old, fix was likely lost
    if(ublox.getFixAge() > 2000){
      data += "No Fix,";
      fix = false;}
    else{
      data += "Fix,";
      fix = true;
    }
    Serial.println(data);}
  /*Serial.print(i);
  Serial2.write(i++);
  if (Serial2.available()) {
    Serial.print(" -> 0x"); Serial.print(Serial2.read(), HEX);
  }
  Serial.println();
  
  delay(10);*/
}
