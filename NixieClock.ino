#include <Wire.h> // Include Libraries
#include <RTClib.h>
#include <MCP41_Simple.h>
#include <EEPROM.h>

RTC_DS3231 rtc; // Define RTC Object
MCP41_Simple vcon; // Define DigiPot Object

// DigiPot Stuff
const float    potValue   = 10000;
const uint16_t wiperResistance = 80;

// Configuration Stuff
int configParameters[] = {1, 0, 1, 1, 0, 0, 2, 120, 10, 5}; // Default Config Parameters(Configured, 24 hour time, colon mode, dim mode, dim by time start, dim by time end, date display period, date display time, anti tube damage)

// Timing Vars
unsigned long previousMillisSep = 0;
unsigned long previousMillisDate = 0;

void setup() {
  Serial.begin(115200); // Serial Init

  DDRH = B01100000; // Separator Control Pins

  vcon.begin(10); // Digital Pot Init
  vcon.setWiper( ((680 - wiperResistance) / potValue) * 255); // Initial Value of 680 Ohm (170v)

  // Initialize RTC if available, if not, enter seperator blink error code, high speed flashing
  if (! rtc.begin()) {
    Serial.println("NO RTC");
    while (1) {
      digitalWrite(8, !(digitalRead(8)));
      digitalWrite(9, !(digitalRead(9)));
      delay(300);
    }
  }

  // Check if RTC battery died, if so, force time to be set before anything else can happen.  Seperators blink at high speed
  if (rtc.lostPower()) {
    while (true) {
      bool breakLoop = false;
      while (Serial.available() > 0) {  // Check if anything has been received on the serial port, add it to the int buffer array
        delay(10); // Buffer fill delay
        byte bufferCount = Serial.available(); // byte with length of recieved bytes
        int serBuffer[bufferCount]; // Generate array with said length
        serBuffer[0] = Serial.read();  // Read the char value into the first array pos
        for (int i = 1; i < bufferCount; i++) {  // Get all the other values as int
          serBuffer[i] = (Serial.read() - '0');
        }
        configMode(serBuffer, bufferCount);  // Send buffer to the config function
        breakLoop = true;

      }
      digitalWrite(8, !(digitalRead(8)));
      digitalWrite(9, !(digitalRead(9)));
      delay(150);
      if (breakLoop) {
        break;
      }
    }
  }

  // Initialize direct port registers in output mode for ports A, C, and L
  DDRA = 0xFF; // Set all pins to Output mode
  DDRC = 0xFF;
  DDRL = 0xFF;

  byte configState = EEPROM.read(0); // Check if EEPROM contains config data, aka the first byte is a 1. If not, load the hardcoded defaults
  if (configState == 0) {
    for (int i = 0; i < 10; i++) {
      EEPROM.update(i, configParameters[i]);  // Write initial config parameters to EEPROM.  Update is used to save unnecessary EEPROM cycles
    }
    Serial.println("Initial EEPROM Programming Done");
  } else {
    for (int i = 0; i < 10; i++) {
      configParameters[i] = EEPROM.read(i); // Load current settings from EEPROM
    }
    Serial.println("Settings Restored from EEPROM");
  }

  if (configParameters[2] == 1) {  // If set to solid on mode, turn seperators on fixed
    PORTH = B01100000;
  } else if (configParameters[2] == 0) {  // Keep pins from floating
    PORTH = B00000000;
  }

}

void loop() {
  DateTime now = rtc.now(); // Get current time from the RTC

  if (now.hour() > 12 && configParameters[1] == 0) {  // Compensates for time from RTC being 24 hour, unless in 24 hour mode
    int hr = now.hour() - 12;
    SetTube(1, nthdig(1, hr));
    SetTube(2, nthdig(0, hr));
  } else if (now.hour() == 0 && configParameters[1] == 0) {  // Changes 0 to 12 at midnight if not in 24 hour mode
    SetTube(1, 1);
    SetTube(2, 2);
  } else {
    SetTube(1, nthdig(1, now.hour()));  // Displays the current hour on the tubes
    SetTube(2, nthdig(0, now.hour()));
  }

  SetTube(3, nthdig(1, now.minute())); // Displays the current minute on the tubes
  SetTube(4, nthdig(0, now.minute()));

  SetTube(5, nthdig(1, now.second())); // Displays the current second on the tubes
  SetTube(6, nthdig(0, now.second()));

  if (configParameters[2] == 2) {  // Timing for toggling seperators.  Counts changes in milliseconds and bases timing on that
    unsigned long currentMillis = millis();
    if ((currentMillis - previousMillisSep) >= 1000) {
      digitalWrite(8, !(digitalRead(8)));
      digitalWrite(9, !(digitalRead(9)));
      previousMillisSep = millis();
    }
  }

  while (Serial.available() > 0) {  // Check if anything has been received on the serial port, add it to the int buffer array
    delay(10); // Buffer fill delay
    byte bufferCount = Serial.available(); // byte with length of recieved bytes
    int serBuffer[bufferCount]; // Generate array with said length
    serBuffer[0] = Serial.read();  // Read the char value into the first array pos
    for (int i = 1; i < bufferCount; i++) {  // Get all the other values as int
      serBuffer[i] = (Serial.read() - '0');
    }
    configMode(serBuffer, bufferCount);  // Send buffer to the config function
  }

  if (configParameters[3] == 1) {  // Automatic brightness compensation based on LDR on analog pin A0.  Varies boost output based on room brightness
    int roomlight = analogRead(A0);  // Get analog value
    int newOhms = ((-4 / 3) * roomlight) + 1613;  // Do some algebra to find new resistance value
    vcon.setWiper( ((newOhms - wiperResistance) / potValue) * 255);  // Update digital pot value
  } else if (configParameters[3] == 2) {
    if ((now.hour() > configParameters[4]) && (now.hour() < configParameters[5])) {
      vcon.setWiper( ((1750 - wiperResistance) / potValue) * 255);
    } else {
      vcon.setWiper( ((680 - wiperResistance) / potValue) * 255);
    }
  }

  if (configParameters[7] > 0) {  // Check if the frequency of date display is > 0, and if so enable it. (0 = disabled)
    unsigned long currentMillisDate = millis();
    unsigned long repeatTime = (configParameters[7] * pow(10, 3)); // Okay, I have no FXXING idea why I have to do it this way.  Multiplying by 1000 breaks things horribly, so multiplying by 10^3
    if ((currentMillisDate - previousMillisDate) >= repeatTime) {  // Timing Stuff
      bool pin8 = digitalRead(8);  // Save current separator status
      bool pin9 = digitalRead(9);
      PORTH = B01100000;  // Turn separators on
      SetTube(1, nthdig(1, now.month()));  // Display date
      SetTube(2, nthdig(0, now.month()));
      SetTube(3, nthdig(1, now.day()));
      SetTube(4, nthdig(0, now.day()));
      SetTube(5, nthdig(1, now.year()));
      SetTube(6, nthdig(0, now.year()));
      delay(configParameters[8] * 1000);  // Hold for date display time
      digitalWrite(8, pin8);  // Set separators back to how they were
      digitalWrite(9, pin9);
      previousMillisDate = millis();  // Reset timer.
    }
  }

  delay(10);  // Stability delay
}

// Functions for main code:

// Set tube function
void SetTube(int tube, int num) {
  int a = PORTA >> 4;  // Get the top 4 bits of the IO ports
  int c = PORTC >> 4;
  int l = PORTL >> 4;
  if (tube % 2 == 0) {  // Even Tubes
    switch (tube) {
      case 2:
        PORTA = (PORTA - (16 * a)) + (16 * num); // Subtract the top value and add a new one based on num
        break;
      case 4:
        PORTC = (PORTC - (16 * c)) + (16 * num);
        break;
      case 6:
        PORTL = (PORTL - (16 * l)) + (16 * num);
        break;
    }
  } else {
    switch (tube) {  // Odd Tubes
      case 1:
        PORTA = (PORTA - (PORTA - (16 * a))) + num;  // Calculate and subtract the bottom value from the top and add a new one
        break;
      case 3:
        PORTC = (PORTC - (PORTC - (16 * c))) + num;
        break;
      case 5:
        PORTL = (PORTL - (PORTL - (16 * l))) + num;
        break;
    }
  }
}


// Digit Parsing
int nthdig(int n, int k) {
  while (n--)
    k /= 10;
  return k % 10;
}

void configMode(int *parameters, byte bufferCount) {  // Takes a pointer to the array and the length of said array as parameters
  switch (parameters[0]) {  // Check the first int and see if it matches any commands
    case 't':
      if (bufferCount == 15) {
        int yr = (parameters[1] * 1000) + (parameters[2] * 100) + (parameters[3] * 10) + parameters[4]; // Single digit to int parsing into vars
        int mnth = (parameters[5] * 10) + parameters[6];
        int dy = (parameters[7] * 10) + parameters[8];
        int hr = (parameters[9] * 10) + parameters[10];
        int mn = (parameters[11] * 10) + parameters[12];
        int sc = (parameters[13] * 10) + parameters[14];
        if ((2000 < yr) && (yr < 2200) && (0 < mnth) && (mnth < 13) && (0 < dy) && (dy < 32) && (-1 < hr) && (hr < 25) && (-1 < mn) && (mn < 60) && (-1 < sc) && (sc < 60)) {  // Bounds Checking
          rtc.adjust(DateTime(yr, mnth, dy, hr, mn, sc));  // Update the RTC if numbers are within bounds
          indicatorMessage(1, 300, 2);  // Give indication that the time was updated
        } else {
          indicatorMessage(2, 500, 3);  // Give indication that the numbers are out of bounds
          break;
        }
      } else {
        indicatorMessage(2, 500, 3);  // Give indication that the number of bytes in the command is incorrect
        break;
      }
      break;
    case 's':
      switch (parameters[1]) {  // Check second parameter to see what to update
        case 0:
          EEPROM.update(0, 0);  // If zero, preform factory reset and alert user to power cycle
          while (true) {
            indicatorMessage(1, 150, 1);
          }
        default:  // Otherwise, set whatever specified parameter to whatever value is given, single digit at the moment
          configParameters[parameters[1]] = parameters[2];
          EEPROM.update(parameters[1], parameters[2]);
          break;
      }
      break;
    default:
      Serial.println("Invalid Configuration Choice");
      break;
  }
}

void indicatorMessage(int combination, int ms, int times) {
  bool pin8 = digitalRead(8);
  bool pin9 = digitalRead(9);
  switch (combination) {
    case 1:
      for (int i = 0; i <= times; i++) {
        PORTH = B00000000;
        delay(ms);
        PORTH = B01100000;
        delay(ms);
      }
      break;
    case 2:
      for (int i = 0; i <= times; i++) {
        PORTH = B00100000;
        delay(ms);
        PORTH = B01000000;
        delay(ms);
      }
      break;
  }
  digitalWrite(8, pin8);
  digitalWrite(9, pin9);
}
