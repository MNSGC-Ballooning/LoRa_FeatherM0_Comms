//Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on


#include <Arduino.h>         //Required before wiring_private.h
#include <wiring_private.h>  //pinPeripheral() function
#include <SD.h>
#include <UbloxGPS.h>
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


//UBLOX GPS
Uart Serial3 (&sercom3, 21, 20, SERCOM_RX_PAD_1, UART_TX_PAD_0); // TX on pin 20
void SERCOM2_Handler()
{
  Serial3.IrqHandler();
}
UbloxGPS ublox = UbloxGPS(&Serial3);
boolean fix = false;
String GPStime = "";
String GPSlocation = "";


//XBee Radio Communications
Uart Serial2 (&sercom4, 24, 23, SERCOM_RX_PAD_3, UART_TX_PAD_2); // TX on pin 23
void SERCOM4_Handler()
{
  Serial2.IrqHandler();
}

char radiopacket[100];

//XBee Object
String ID = "LORA";
RelayXBee xBee = RelayXBee(&Serial2, ID);


//SD Logging
File datalog;
char filename[] = "LORA00.csv";
bool SDactive = false;

int16_t packetnum = 0;  //Packet counter, we increment per xmission



void setup() {

  //Serial Connection Pin Setup (SOFTWARE SERIAL)
  pinPeripheral(20, PIO_SERCOM);
  pinPeripheral(21, PIO_SERCOM);
  pinPeripheral(23, PIO_SERCOM_ALT);
  pinPeripheral(24, PIO_SERCOM_ALT);

  //LoRa Radio Setup
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  //Serial and Device Setup
  Serial.begin(9600);
  Serial3.begin(UBLOX_BAUD);
  Serial2.begin(XBEE_BAUD);

  //XBee Setup
  xBee.init('A');
  xBee.enterATmode();
  xBee.atCommand("ATID000D");  //Changes PAN ID to "000D"
  xBee.atCommand("ATDL1");
  xBee.atCommand("ATMY0");
  xBee.exitATmode();

  //SD Logging Setup
  Serial.print("Initializing SD card...");
  if(!SD.begin(SDchipSelect))                                 //attempt to start SD communication
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
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  
  //Communication Pin Definition (Software Serial for XBee and UBLOX)
  rf95.setTxPower(23, false);
  ublox.init();
  delay(200);

  //UBLOX Setup
  Serial.println("Testing UBLOX Airborne Mode");
  for (byte j = 0; j<=50; j++) {
    if (ublox.setAirborne()) {
      Serial.println("Airborne mode successfully set.");
      break;
    }
    else if (j ==50)
      Serial.println("WARNING: Failed to set to air mode (50 attemtps). Altitude data may be unreliable.");
    else
      Serial.println("Error: Airborne mode set failure. Reattempting...");
  }

  while(Serial3.available()==0) {
    Serial.println("UBLOX FAILURE");
  }

  while (Serial2.available()==0) {
    Serial.println("XBEE FAILURE");
  }

}

void loop() {

  //UBLOX Get Data
  ublox.update();

  Serial.println("Test");

  //Data Loop
  if(millis()%2000 == 0)
  {
     GPStime = String(ublox.getHour()-5) + ":" + String(ublox.getMinute()) + ":" + String(ublox.getSecond());
     GPSlocation = "Number of Sats: " + String(ublox.getSats()) + ", Lat/Lon: " + String(ublox.getLat(),10) + "/" + String(ublox.getLon(),10);
     if(ublox.getFixAge() > 3000){
       fix = false;}
     else{
       fix = true;
    }
  }
  
  saveAll();
  radioTransmit();

  Serial.println(GPStime);
  Serial.println(GPSlocation);
  Serial.println(fix);
  delay(1000);
  
}



void saveAll() {
  
  String saveData = GPStime + "," + GPSlocation + "," + String(ublox.getAlt_meters()) + "," + String(fix) + "," + String();
  datalog = SD.open(filename, FILE_WRITE);
  datalog.println(saveData);
  datalog.close();
  
}



void radioTransmit() {
  
  if(Serial2.available()>0){
    int availableBytes = Serial2.available();
    for(int i=0; i<availableBytes; i++) {
      radiopacket[i] = Serial2.read();
    }
  itoa(packetnum++, radiopacket+13, 10); // itoa means integer to ascii
  
  radiopacket[19] = 0;



  delay(10);
  rf95.send((uint8_t *)radiopacket, 20);
  
  delay(10);
  rf95.waitPacketSent();
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  Serial.println(radiopacket);
  }
}
