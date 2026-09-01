/*
 * Demo for RF remote switch receiver.
 * For details, see NewRemoteReceiver.h!
 *
 *
 * When run, this sketch waits for a valid code from a new-style the receiver,
 * decodes it, and retransmits it after 5 seconds.
 * 
 *  Notes: Arduino only!!!
 *  
 *  https://github.com/1technophile/NewRemoteSwitch
 *  https://github.com/LSatan/SmartRC-CC1101-Driver-Lib
 *  ----------------------------------------------------------
 *  Mod by Little Satan. Have Fun!
 *  ----------------------------------------------------------
 */

#include <ELECHOUSE_CC1101_SRC_DRV.h>
//#include <NewRemoteReceiver.h>
#include <NewRemoteTransmitter.h>

unsigned long learnedAddress=13327346;
byte learnedUnit=2;
int learnedPeriod=259;

//esp32-s3-box
int gdo2Pin=13;
int sckPin=9;
int mosiPin=42;
int god1Pin=12;
int gdo0Pin=39;
int csnPin=40;

void setup() {
  delay(1000);
  Serial.begin(115200);  
  Serial.println("Setting up cc1101");
  
//CC1101 Settings:                (Settings with "//" are optional!)
  ELECHOUSE_cc1101.setSpiPin(sckPin,/*miso*/god1Pin,mosiPin,csnPin);
  ELECHOUSE_cc1101.setGDO(gdo0Pin,gdo2Pin);

//CC1101 Settings:                (Settings with "//" are optional!)
  ELECHOUSE_cc1101.Init();            // must be set to initialize the cc1101!
//ELECHOUSE_cc1101.setRxBW(812.50);  // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.
  ELECHOUSE_cc1101.setPA(10);       // set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12)   Default is max!
  ELECHOUSE_cc1101.setMHZ(433.92); // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.

  ELECHOUSE_cc1101.SetTx();  // set Receive on 
  delay(1000);
  Serial.println("Sending 13327346, 2, on");
  NewRemoteTransmitter transmitter(learnedAddress, gdo0Pin, learnedPeriod);
  transmitter.sendUnit(2, 1);
}

void loop() {
}



  