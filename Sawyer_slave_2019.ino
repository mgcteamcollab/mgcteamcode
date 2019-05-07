// allows values to be represented as floats and bytes so they can be sent across the wire
union FloatData { //should allow us to access the same data as either a float or an
  byte bytes[4];  //array of 4 bytes, which will allow us to send the floats one byte
  uint32_t fval;     //at a time.
};

// initializes arrays with dummy data... can be used for testing
FloatData pressures[6] = {0xA31, 0xB32, 0xC33, 0xD34, 0xE35, 0xF36};
FloatData flows[6] = {0x03A, 0x13B, 0x23C, 0x33D, 0x43E, 0x53F};

// sets analog and digital pins, see arduino board schematics
int APins[6] = {0,1,2,3,6,7};
int DPins[6] = {9,8,7,6,5,4};

// sets number of bytes to send (32 is max without issues for current protocol)
const int bytesRequested = 24;
byte toSend[48];
// initializes bytes for testing, commented out for now
// byte toSend[bytesRequested];// = {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA};
// byte toSend[48]= {1, 12, 4, 15, 63, 155, 252, 55, 67, 12, 3, 145, 202, 155, 34, 102, 78, 212, 115, 32, 65, 90, 189, 233, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48};


// gets wire library included
#include <Wire.h>

void setup()
{
  
  Wire.begin(10);                // join i2c bus with address #23
  // this will initialize wire upon requestEvent
  Wire.onRequest(requestEvent); // register event
  Serial.begin(9600);
  // sets input pints to pins on board, see arduino pro mini schematics
  pinMode(9, INPUT);
  pinMode(8, INPUT);
  pinMode(7, INPUT);
  pinMode(6, INPUT);
  pinMode(5, INPUT);
  pinMode(4, INPUT);
}

void loop()
{
  // tried this in a loop and it did not work
  // have to set values specifically
  pressures[0].fval = pressureFunction(7);
  pressures[1].fval = pressureFunction(6);
  pressures[2].fval = pressureFunction(3);
  pressures[3].fval = pressureFunction(2);
  pressures[4].fval = pressureFunction(1);
  pressures[5].fval = pressureFunction(0);
  flows[0].fval = flowRate(4);
  flows[1].fval = flowRate(5);
  flows[2].fval = flowRate(6);
  flows[3].fval = flowRate(7);
  flows[4].fval = flowRate(8);
  flows[5].fval = flowRate(9);
  Serial.print(flows[0].fval);
  Serial.print(flows[1].fval);
  Serial.print(flows[2].fval);
  Serial.print(flows[3].fval);
  Serial.print(flows[4].fval);
Serial.println(flows[5].fval);
  // polls flow and pressure pins for sensor values
   for (int i=0; i>6; i++)
   {
     pressures[i].fval = pressureFunction(APins[i]);
     flows[i].fval = flowRate(DPins[i]);   
   }
 // gives a slight delay for each loop
  delay(10);
}

// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent()
{
 for (int i = 0; i < 6; i++) // i < 6 because loops runs once per filter monitored
  {                           // each time through the loop, we'll add 4 bytes of data, 2 per sensor

     // gets pressure values from loop function
     // adds these values to toSend array
     for (int k = 0; k < 2; k++) {
      toSend[(i*4)+k] = pressures[i].bytes[k];
    }

    // gets flow values from loop function
    // adds these values to toSend array
    for (int j = 0; j < 2; j++) {
      toSend[(i*4)+2+j] = flows[i].bytes[j];
    }
   }

  // writes toSend to wire, 24 bytes
  // this is picked up by master
  Wire.write(&toSend[0], bytesRequested);
  
}


// gathers flow rate values
float flowRate (int i)
{  

// sets time to grab a one second sample of flow sensor values
// this is because sensor values are output as frequencies
// see data sheet for honeywell sensors
long oldTime = millis();
long newTime = millis();
int gateTime = 1000;
int count=0;
float flow = 0;
  // reads state of pin and counts changes to state for length of gate time
  // (1 second because freq is measured in hz)
  while((newTime - oldTime) < gateTime)
  {
    int currentState = digitalRead(i);
    // loops until voltage on pin is changed, signalling a cycle
    while (digitalRead(i) == currentState && (newTime - oldTime) < gateTime)
    {
      newTime = millis();
    }
    newTime= millis();
    currentState=digitalRead(i);
    // adds count to measure state change
    count++;
  }
 // frequency is count/2 because one cycle is two state changes (see definition of wave)
 flow = count/2; 
 flow = flow;
 return flow;
}

// gets pressure values from sensor
// sensor outputs these values as voltages see data sheet
float pressureFunction (int i)
{
// initializes average pressure variable
float Apressure = 0;
// this was placed in a loop, but it did not work for some reason
// have to manually call each pressure value
float pressure0 = analogRead(i);
float pressure1 = analogRead(i);
float pressure2 = analogRead(i);
float pressure3 = analogRead(i);
float pressure4 = analogRead(i);
float pressure5 = analogRead(i);
float pressure6 = analogRead(i);
float pressure7 = analogRead(i);
float pressure8 = analogRead(i);
float pressure9 = analogRead(i);

// gets average pressure from 10 readings... I don't know why this is necessary was left by the original MGC team
Apressure = (pressure0 + pressure1 + pressure2 + pressure3 + pressure4 + pressure5 + pressure6 + pressure7 + pressure8 + pressure9)/10;
// this is necessary for some reason? I don't know anymore
Apressure = Apressure;
return Apressure;
}
