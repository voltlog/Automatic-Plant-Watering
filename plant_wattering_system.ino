// Arduino Automatic Plant Wattering System
// Copyright (c) 2015 Florin Cocos - VoltLog
// Website http://www.voltlog.com
// Youtube https://www.youtube.com/c/voltlog
// Released under MIT license http://opensource.org/licenses/mit-license.php

#include <Wire.h>
#include <TimerOne.h>
#include <RTClib.h>


// Set alarm trigger hour and minute in 24h format
const int alarm1_hour = 19;
const int alarm1_minute = 0;
const int alarm2_hour = 6;
const int alarm2_minute = 0;

// Set pump ON time in seconds multiplied by 10 because the timer interrupt actually triggers each 100mS
// This should be adjusted for your particular pump
const int pumpTime = 15 * 10;

// Set battery cut off voltage
const float cutoff_voltage = 10.6;

// Map the outputs
const int greenLED = 6;
const int redLED = 5;
const int buzzer = 3;
const int relay = 4;
const int sensePin = A0;    // select the input pin for battery voltage sense

// Some variables used in the code are initialized here
int alarm1_day_log = 0;
int alarm2_day_log = 0;
int previous_hour = 0;

volatile unsigned int count = 0; // using volatile for shared variables
volatile boolean isPumpON = false; // using volatile for shared variables
volatile boolean cmdReceived = false;  // whether UART cmd char is received
volatile char rxchar = 0; // this will hold the uart rx char

// initialize RTC
RTC_BQ32000 RTC;

void setup () {
// Initialize required pins as outputs.
pinMode(greenLED, OUTPUT);
pinMode(redLED, OUTPUT);
pinMode(relay, OUTPUT);

// Make sure everything is off
digitalWrite(greenLED, LOW);   // turn off the LED
digitalWrite(redLED, LOW);   // turn off the LED
digitalWrite(relay, LOW);   // turn off the pump

// Configure Timer1 interrupt to fire every 100mS & attach driverPump()
Timer1.initialize(100000);
Timer1.attachInterrupt(drivePump);

// Configure serial
Serial.begin(57600);
Wire.begin();

RTC.begin();

// following line sets the RTC to the date & time this sketch was compiled
//RTC.adjust(DateTime(__DATE__, __TIME__));

// The BQ32000 can generate a square wave output on the IRQ pin, uncomment one of
// the following lines to use it:
RTC.setIRQ(0); // IRQ square wave disabled
//RTC.setIRQ(1); // 1Hz square wave on IRQ
//RTC.setIRQ(2); // 512Hz square wave on IRQ

// If the IRQ square wave output is disabled this method will set the IRQ pin's
// state to either HIGH or LOW:
RTC.setIRQLevel(HIGH);

RTC.setCalibration(0);
// Set to a value in the range [-31,31] to adjust clock frequency
// as needed. Refer to Table 13 in the BQ32000 datasheet from here:
//  http://www.ti.com/product/bq32000

// The BQ32000 includes a trickle charge circuit that can be enabled if using a
// super capacitor for backup power; uncomment one of the following lines if needed.
RTC.setCharger(0); // Disabled
//RTC.setCharger(1); // Charge voltage = VCC-0.5V
//RTC.setCharger(2); // Charge voltage = VCC
// *Make sure the charge circuit is disabled if using a battery for backup power!*

Serial.print("System ready! \r\n");

// Print current time on startup
printTime();
}

// This function will check the battery voltage
bool checkBattery() {
// Read ADC value
int adc = analogRead(A0);

// Resistor divider ratio 75K / 20K measured to be 4.767772
// ADC step calculated to be VCC(3.28V)/1024 = 0.003203
// Vbattery = divider_ratio * adc_step * adc_value
// adc_step * adc_value has been pre-calculated to be 0.015271173716
float voltage = adc * 0.0152711;

// if measured voltage is above cutoff voltage return true else return false
if (voltage > cutoff_voltage) return true;
else return false;
}

// This function is attached to timer interrupt and will handle the water pump
void drivePump(void) {
if (isPumpON)
{
  if (count < pumpTime)
  {
    count++;
  }
  else
  {
    //finished pumpTime
    count = 0;
    isPumpON = false;
    digitalWrite(greenLED, LOW);   // turn the LED off
    digitalWrite(relay, LOW);   // turn the pump off
    Serial.print("Finished pumping! \r\n");
  }
}
}

// Handy function for printing current RTC time on request
void printTime(void) {
// Get current time from RTC
DateTime now = RTC.now();

Serial.print("System time is: ");
Serial.print(now.year(), DEC);
Serial.print('/');
Serial.print(now.month(), DEC);
Serial.print('/');
Serial.print(now.day(), DEC);
Serial.print(' ');
Serial.print(now.hour(), DEC);
Serial.print(':');
Serial.print(now.minute(), DEC);
Serial.print(':');
Serial.print(now.second(), DEC);
Serial.println();
}

// SerialEvent occurs whenever a new data comes in the hardware serial RX.
void serialEvent() {
while (Serial.available()) {
  // store the new byte:
  rxchar = (char)Serial.read();
  // signal the main loop and let it take care of the response
  cmdReceived = true;
}
}

// This function will sound the buzzer 3 times when called
void alarm(void) {
//generate 3 beeps
digitalWrite(buzzer, HIGH);
delay(100);
digitalWrite(buzzer, LOW);
delay(25);
digitalWrite(buzzer, HIGH);
delay(100);
digitalWrite(buzzer, LOW);
}

void loop () {
// Get current time from RTC
DateTime now = RTC.now();

//drive pump only if battery is above cutoff voltage
if (checkBattery()) {
  if (now.hour() == alarm1_hour && now.day() != alarm1_day_log) { // hour matches, but have we had the alarm already today ?
    if (now.minute() == alarm1_minute) {
      alarm1_day_log = now.day();  // Log alarm day
      digitalWrite(greenLED, HIGH);   // turn the LED on
      digitalWrite(relay, HIGH);   // turn the pump on
      Serial.print("Alarm 1 on! \r\n");
      isPumpON = true;
    }
  }

  if (now.hour() == alarm2_hour && now.day() != alarm2_day_log) { // hour matches, but have we had the alarm already today ?
    if (now.minute() == alarm2_minute) {
      alarm2_day_log = now.day();  // Log alarm day
      digitalWrite(greenLED, HIGH);   // turn the LED on
      digitalWrite(relay, HIGH);   // turn the pump on
      Serial.print("Alarm 2 on! \r\n");
      isPumpON = true;
    }
  }
} else  if (now.hour() != previous_hour && now.minute() == 0) {
  // sound the battery alarm every hour, minute 0, for a total of 1 minute, at 10 seconds interval
  alarm();
  delay(5000);
  delay(5000);
}

// Process incoming commands over UART
if (cmdReceived) {
  switch (rxchar) {
    case 't':  // print time cmd
      printTime();
      cmdReceived = false;
      break;
    case 'm':  // manual trigger cmd
      digitalWrite(relay, HIGH);   // turn the pump on
      digitalWrite(greenLED, HIGH);   // turn the LED on
      Serial.print("Manual trigger, pump on! \r\n");
      isPumpON = true;
      cmdReceived = false;
      break;
  }
}

}
