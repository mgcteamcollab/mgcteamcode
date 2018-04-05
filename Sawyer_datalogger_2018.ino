

/**
* This code is designed to be run on an Arduino Uno with an Adafruit datalogger shield.
* To work properly it should be connected to 4 other, sensor-polling Arudinos via I2C,
* to the forward pump relay via pin 3 and to the backflush relay via pin 4. This will run the 
* forward pump for 59 minutes out of the hour and record data on the SD card once per minute. 
* Then it wil backflush the system for a minute and record data once per second. This data will
* be inflated by 1000 in order to cast it to an int with precision and then be able to add it to
* the string printed on the SD card. This should be corrected by dividing the output by 1000.
*/


// adjusts how long system backflushes for (in seconds)
int backflushDuration = 60;

// see documentation on unions... allows data to be represented in two ways
union FloatData { //should allow us to access the same data as either a float or an
  byte bytes[4];  //array of 4 bytes, which will allow us to send the floats one byte
  uint32_t fvalue;   //at a time.
} value;

// libraries aka extra code required to call certain functions
// ex: time and wire libraries are needed
// see documentation on what are libraries

//#include <DS1307RTC.h>
#include <SD.h> 
#include <Wire.h>
#include <RTClib.h>
#include <TimeLib.h>
#include <SPI.h>

// initializes SD chip
const int chipSelect = 10; // required by SD library

// initialize variables to be used throughout code
int currentMin; // These are used in the control code to keep track of when 
int prevMin;    // to pump in which direction.
int currentSec;
int prevSec;
int secCounter = 0;
boolean LPMcheck = false;

// more variables... these will be used in setup
int numMicros = 4; // number of sensor-polling micros 
int filtersPerMicro = 6;
int microI2CAdresses[] = {9 , 10 , 11, 12}; // MAKE SURE EACH MICRO HAS ONE OF THESE ADDRESSES
                                            // AND THAT NONE ARE REPEATED
int forwardRelayPin = 5;
int backflushRelayPin = 6;
int LPMchecklight = 7;

String dataToWrite;


#define numBytesRequested (2 * filtersPerMicro * 2) // Two sensors per filter * six filters per micro * 2 bytes
DateTime tNow;
RTC_DS1307 rtc;

void setup() {
    // delay needed to allow full boot? rtc.begin is buggy without delay
    delay(3000);
    // rtc.begin starts real time clock
    rtc.begin();
    Serial.begin(9600);
    // sets digital pins on arduino to variable values
    // see board layout schematics online 
    pinMode(10, OUTPUT);
    pinMode(forwardRelayPin, OUTPUT);
    pinMode(backflushRelayPin, OUTPUT);
    pinMode(LPMchecklight, OUTPUT);
    digitalWrite(10, HIGH);
    // checks if SD card is in
    if (!SD.begin(chipSelect)) 
    {
        Serial.println("Card failed, or not present");
        // don't do anything more:
        return;
    }
    Serial.println("card initialized.");
    // creates an excel sheet (comma seperated values for each cell entry)
    char filename[] = "DATALOG.CSV";
    File logfile = SD.open(filename, FILE_WRITE);
    if (! logfile) 
    {
        Serial.println("couldnt create file");
    }
    logfile.close();
    // bug with Wire library requires it to be called a couple times...
    // this is a hack-around fix... should just be one call to begin
    Wire.begin();
    Wire.begin();
    Wire.begin();
    Wire.begin();
    if (! (rtc.begin())) 
    {
        Serial.println("RTC failed");
    }
    currentMin = tNow.minute(); 
    prevMin = currentMin;
    
    /***You may need to use this line the first time you run***/
    
    // rtc.adjust(DateTime(__DATE__, __TIME__)); // Used to set RTCC to complile time
    // adjust below to correct date and time when re-uploading or re-starting system
    // rtc.set(now());

    // initializes pump controls, turning forward on and backflush off
    // LPM checklight is initialized at low... this is for LPM functionality
    // as defined in the project reports (flow cannot be below 1 LPM)
    digitalWrite(forwardRelayPin, HIGH);
    digitalWrite(backflushRelayPin, LOW);
    digitalWrite(LPMchecklight, LOW);
}
 
// loops infinitely, only breaking out for function calls
void loop() {
  dataToWrite = "";
  while(currentMin == prevMin)
  {
    tNow = rtc.now();
    currentMin = tNow.minute();
    delay(5000);
  }

  recordData();
  prevMin = currentMin;

  // adjust when backflush occurs... this logic can be changed relatively easily
  // could be used to backflush at any interval of time
  // may change to backflush once per day
  if (currentMin == 0 || currentMin == 20 || currentMin == 40) {
    backFlush();
  }
  
  // for some reason data was not being recorded without conditional logic
  // another hack-around... 
  if (true) {
    recordData();
  }
}

/*
Creates a string with the micro address, the time, the pressure and flow of each filter
in turn. This string is then written to datalog.csv
*/
void recordData() {
    FloatData value;
    dataToWrite = "";
    LPMcheck = false;
    // runs loop for each slave chip gathering its data
    for(int i = 0; i < numMicros; i++)
    {
      // polls real time clock for time
      tNow = rtc.now();
      dataToWrite.concat(String(microI2CAdresses[i]));    
      dataToWrite.concat(",");
      dataToWrite.concat(String(tNow.day()));
      dataToWrite.concat('/');
      dataToWrite.concat(String(tNow.month()));
      dataToWrite.concat('/');
      dataToWrite.concat(String(tNow.year()));
      dataToWrite.concat(" ");
      dataToWrite.concat(String(tNow.hour()));
      dataToWrite.concat(':');
      dataToWrite.concat(String(tNow.minute()));
      dataToWrite.concat(':');
      dataToWrite.concat(String(tNow.second()));

      // makes request to micro at address specified by value at i index
      // in the micro_addresses array. 24 bytes are requested 
      // see wire communication master/slave documentation at arduino.com
      Wire.requestFrom(microI2CAdresses[i], numBytesRequested);
      while(Wire.available())
        {
          // data for pressure and flow and written as float values
          // which are also read as 4 bytes (see union comment at top of code)
          // only data from last two bytes is required as two bytes hold values
          // 0 through 256*256... using any more bytes causes failure
          // as wire cannot send more than 32 bytes apparently... 
          // 4 slaves * 6 sensors each == 24
            dataToWrite.concat(",");
          for(int i = 0; i < 2; i++) {
              value.bytes[i] = Wire.read();
         }
         
      // this logic was created to track flow rate of filters...
      // if flow rate drops below roughly 1LPM in any filter, 
      // the output pin for LPMchecklight will be placed at 5V
      // this should light an LED
      int tempValue = (int) ((value.bytes[0])+ value.bytes[1]*255);
        if (tempValue < 22 && LPMcheck) {
          digitalWrite(LPMchecklight, HIGH);
        }

      // casts int value to a string so it can be added 
      // to dataToWrite variable
      String tempString = String(tempValue);
      dataToWrite.concat(tempString);
      LPMcheck = true;
    }
    dataToWrite.concat("\n");

    // dont know why this logic is here... should be able to just put this
    // all outside the for loop. Leaving it here in case there was a reason
    // the prior class wrote it. 
    
    if (i == (numMicros - 1)) {
      File dataFile = SD.open("datalog.csv", FILE_WRITE);
  
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataToWrite);
      dataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("non-existent");
    }
  }
    }
}

/*
Backflushes for 60 seconds 
*/
void backFlush()
{
  tNow = rtc.now();
  prevSec = 0;
  currentSec = prevSec;
  // initializes pump control pins 
  // sets backflush pump to on
  digitalWrite(forwardRelayPin, LOW);
  digitalWrite(backflushRelayPin, HIGH);

  secCounter = 0;

  // loops for duration of backflushDuration
  while(secCounter < backflushDuration) {
   while(currentSec == prevSec) {
    tNow = rtc.now();
    currentSec = tNow.second();
    delay(300);
  }
  prevSec = currentSec;

  secCounter++;
  }

  // once loop has executed for the time specified in backFlush duration
  // resets the forward and backflush pins
  digitalWrite(forwardRelayPin, HIGH);
  digitalWrite(backflushRelayPin, LOW);
}

 
 
 
 

