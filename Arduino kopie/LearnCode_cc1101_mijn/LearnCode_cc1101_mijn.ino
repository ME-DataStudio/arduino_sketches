/*
 * Demo for RF remote switch receiver. 
 * This example is for the new KaKu / Home Easy type of remotes!
 *
 * For details, see NewRemoteReceiver.h!
 *
 * With this sketch you can control a LED connected to digital pin 4,
 * after the sketch learned the code. After start, the LED starts to blink,
 * until a valid code has been received. The led stops blinking. Now you
 * can control the LED with the remote.
 *
 * Note: only unit-switches are supported in this sketch, no group or dim.
 *
 * Arduino only!
 *
 *  https://github.com/1technophile/NewRemoteSwitch
 *  https://github.com/LSatan/SmartRC-CC1101-Driver-Lib
 *  ----------------------------------------------------------
 *  Mod by Little Satan. Have Fun!
 *  ----------------------------------------------------------
 *
 */

#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <NewRemoteReceiver.h>



//int pin = 0; // int for Receive pin.
//int led = 4; // pin for Led.


//esp32-s3-box
int gdo2Pin=10;
int sckPin=14;
int mosiPin=13;
int god1Pin=11;
int gdo0Pin=9;
int csnPin=12;


//RP2040 pins SPI0
//byte sckPin = 18;   
//byte god1Pin = 16; //=miso
//byte mosiPin = 19;
//byte csnPin = 17;
//int gdo0Pin = 20;
//int gdo2Pin = 21;

boolean codeLearned = false;
unsigned long learnedAddress;
byte learnedUnit;
NewRemoteCode remoteCode;

void setup() {
  delay(1000);
  Serial.begin(115200);  
  Serial.println("Setting up cc1101");
  
//  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
//CC1101 Settings:                (Settings with "//" are optional!)
  ELECHOUSE_cc1101.setSpiPin(sckPin,/*miso*/god1Pin,mosiPin,csnPin);
  ELECHOUSE_cc1101.setGDO(gdo0Pin,gdo2Pin);
  ELECHOUSE_cc1101.Init();            // must be set to initialize the cc1101!
//ELECHOUSE_cc1101.setRxBW(812.50);  // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.
//ELECHOUSE_cc1101.setPA(10);       // set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12)   Default is max!
  ELECHOUSE_cc1101.setMHZ(433.92); // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
 
  ELECHOUSE_cc1101.SetRx();    // set Receive on
  
  // LED-pin as output
  //pinMode(led, OUTPUT);

  // Init a new receiver on interrupt pin 0, minimal 2 identical repeats, and callback set to processCode.
  NewRemoteReceiver::init(gdo2Pin, 2, processCode);
}

void loop() {
  // Blink led until a code has been learned

  //if (!codeLearned) {
  //  digitalWrite(13, HIGH);
  //  delay(500);
  //  digitalWrite(13, LOW);
  //  delay(500);
  //}
}

// Callback function is called only when a valid code is received.
void processCode(NewRemoteCode receivedCode) {
  // A code has been received.
  // Do we already know the code?
  Serial.println(receivedCode.address);
  Serial.println(receivedCode.unit);
  Serial.println(receivedCode.switchType);
  Serial.println(receivedCode.period);
  if (!codeLearned) {
    // No! Let's learn the received code.
    learnedAddress = receivedCode.address;
    learnedUnit = receivedCode.unit;
    codeLearned = true;   
    Serial.println(learnedAddress); 
  } else {
    // Yes!    
    // Is the received code identical to the learned code?
    if (receivedCode.address == learnedAddress && receivedCode.unit == learnedUnit) {
      // Yes!
      // Switch the LED off if the received code was "off".
      // Anything else (on, dim, on_with_dim) will switch the LED on.
      if (receivedCode.switchType == NewRemoteCode::off) {
        //digitalWrite(13, LOW);
      } else {
        //digitalWrite(13, HIGH);
      }
    }
  }
}
