//Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on


#include <Arduino.h>         //Required before wiring_private.h
#include <wiring_private.h>  //pinPeripheral() function
#include <SPI.h>
#include <RH_RF95.h>
#include <RelayXBee.h>



// Feather m0 w/wing 
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     6    // "D"
#define RF95_FREQ 433.0
#define SDchipSelect 4


//Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);


char radiopacket[100];



void setup() {


  //LoRa Radio Setup
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  //Serial and Device Setup
  Serial.begin(9600);
 
  //Manual Reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  //While Loop Blocking Startup if LoRa not working
  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  //While Loop Blocking Startup if LoRa not working
  //Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  
  //Communication Pin Definition (Software Serial for XBee and UBLOX)
  rf95.setTxPower(23, false);
  delay(200);

  int16_t packetnum = 0;  //Packet counter, we increment per xmission

}



void loop() {

  if(rf95.available()>0){
    
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.recv(buf, &len)) {
      
      RH_RF95::printBuffer("Received: ", buf, len);
      Serial.println((char*)buf);
      
    }
  }

}
