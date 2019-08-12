#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <SD.h>
#include <UbloxGPS.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <RelayXBee.h>
#include <RHGenericSPI.h>

// Feather m0 w/wing 
#define RFM95_RST     11   // "A"
#define RFM95_CS      10   // "B"
#define RFM95_INT     6    // "D"
//
#define RF95_FREQ 433.0

#define chipSelect 4

//RHGenericSPI spi(RFM95_CS,RFM95_RST,RF95_FREQ);
// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

//Dedicating pins 20 and 21 to Serial3 for Ublox. Effectively SoftwareSerial
Uart Serial3 (&sercom3, 5, 6, SERCOM_RX_PAD_1, UART_TX_PAD_0); // TX on pin 20
void SERCOM3_Handler(){Serial3.IrqHandler();}

UbloxGPS ublox = UbloxGPS(&Serial3);
boolean fix = false;
String GPStime = "";
String GPSlocation = "";
char radiopacket[100];

//Dedicating pins 5 and 22 to Serial3 for Ublox. Effectively SoftwareSerial
Uart Serial2 (&sercom2, 22, 23, SERCOM_RX_PAD_3, UART_TX_PAD_0); // TX on pin 22
void SERCOM2_Handler(){Serial2.IrqHandler();}
String ID = "LORA";
RelayXBee xBee = RelayXBee(&Serial2, ID);

//SD logging onboard the feather M0
File datalog;
char filename[] = "LORASS.csv";
bool SDactive = false;

void setup()
{
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);
  while(!Serial);
  Serial3.begin(UBLOX_BAUD);
  Serial2.begin(XBEE_BAUD);

  xBee.init('A');
  xBee.enterATmode();
  xBee.atCommand("ATDL1"); //So stack xbees communicate with the relay xbee.
  xBee.atCommand("ATMY0"); // ""
  xBee.exitATmode();
  
  Serial.print("Initializing SD card...");
  if(!SD.begin(chipSelect))                                 //attempt to start SD communication
    Serial.println("Card failed, or not present");          //print out error if failed; remind user to check card
  else {                                                    //if successful, attempt to create file
    Serial.println("Card initialized.\nCreating File...");
    for (byte i = 0; i < 100; i++) {                        //can create up to 100 files with similar names, but numbered differently
      filename[4] = '0' + i/10;
      filename[5] = '0' + i%10;
      if (!SD.exists(filename)) {                           //if a given filename doesn't exist, it's available
        datalog = SD.open(filename, FILE_WRITE);            //create file with that name
        SDactive = true;                                    //activate SD logging since file creation was successful
        Serial.println("Logging to: " + String(filename));  //Tell user which file contains the data for this run of the program
       // xBee.println("Logging to: " + String(filename));
        break;                                              //Exit the for loop now that we have a file
      }
    }
    if (!SDactive) Serial.println("No available file names; clear SD card to enable logging");
  }
  if(SDactive)
  {
    datalog = SD.open(filename, FILE_WRITE);
    datalog.println("Beginning data recording.");
    datalog.close();
  }
  
  Serial.println("Feather LoRa TX Test!");
 
  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  
  while (!rf95.init()) {  // Make sure to connect 'A' to 'RST' and 'B' to 'CS' on the FeatherWing
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1);
  }
  Serial.println("LoRa radio init OK!");
 
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
 
  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

  pinPeripheral(20, PIO_SERCOM); // RX pin going to ublox
  pinPeripheral(21, PIO_SERCOM); // TX pin going to ublox
  pinPeripheral(5, PIO_SERCOM); // RX pin going to xbee
  pinPeripheral(22, PIO_SERCOM); // TX pin going to xbee
  ublox.init();
  delay(200);
  
  Serial.print("testing");
  for (byte j = 0; j<50; j++) {
    if (ublox.setAirborne()) {
      Serial.println("Air mode successfully set.");
      break;
    }
    else if (j ==50)
      Serial.println("WARNING: Failed to set to air mode (50 attemtps). Altitude data may be unreliable.");
    else
      Serial.println("Error: Air mode set unsuccessful. Reattempting...");
  }
}

int16_t packetnum = 0;  // packet counter, we increment per xmission

void saveAll()
{
  String saveData = GPStime + "," + GPSlocation + "," + String(ublox.getAlt_meters()) + "," + String(fix) + "," + String();
  datalog = SD.open(filename, FILE_WRITE);
  datalog.println(saveData);
  datalog.close();
}

void radioTransmit()
{
  //delay(1000); // Wait 1 second between transmits, could also 'sleep' here!
  //Serial.println("Transmitting..."); // Send a message to rf95_server

  if(Serial2.available()>0){
    int availableBytes = Serial2.available();
    for(int i=0; i<availableBytes; i++)
    {
      radiopacket[i] = Serial2.read();
    }
    //radiopacket[] = Serial.read();
  //char radiopacket[20] = "Hello World #      ";
  itoa(packetnum++, radiopacket+13, 10); // itoa means integer to ascii
  
  Serial.print("Sending "); Serial.println(radiopacket);
  radiopacket[19] = 0;
  
  Serial.println("Sending...");
  delay(10);
  rf95.send((uint8_t *)radiopacket, 20);
  
  Serial.println("Waiting for packet to complete..."); 
  delay(10);
  rf95.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
 
  Serial.println("Waiting for reply...");
  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len))
   {
      Serial.print("Got reply: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);    
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
  else
  {
    Serial.println("No reply, is there a listener around?");
  }
}}

void loop()
{
  ublox.update();

  if(millis()%1500 == 0)
  {
     GPStime = String(ublox.getHour()-5) + ":" + String(ublox.getMinute()) + ":" + String(ublox.getSecond());
     GPSlocation = "Number of Satellites " + String(ublox.getSats()) + " Lat/Lon: " + String(ublox.getLat(),10) + "/" + String(ublox.getLon(),10);
     if(ublox.getFixAge() > 3000){
       fix = false;}
     else{
       fix = true;
    }
  }
  
  saveAll();
  radioTransmit();
}
