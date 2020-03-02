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
int configParameters[] = {1, 0, 1, 1, 0, 2, 30, 5}; // Default Config Parameters(Configured, 24 hour time, colon mode)

// Timing Vars
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200); // Serial Init

  pinMode(8, OUTPUT); // Seperator Control Pins
  pinMode(9, OUTPUT);

  // Initialize RTC if available, if not, enter seperator blink error code, high speed flashing
  if (! rtc.begin()) {
    Serial.println("NO RTC");
    while (1) {
      digitalWrite(8, !(digitalRead(8)));
      digitalWrite(9, !(digitalRead(9)));
      delay(300);
    }
  }

  vcon.begin(10); // Digital Pot Init
  vcon.setWiper( ((680 - wiperResistance) / potValue) * 255); // Initial Value of 680 Ohm (170v)

  // Check if RTC battery died
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, send time in format: YYYYMMDDHHMMSS");  // Request time to be sent in format
  }

  // Initialize direct port registers in output mode for ports A, C, and L
  DDRA = 0xFF; // Set all pins to Output mode
  DDRC = 0xFF;
  DDRL = 0xFF;

  byte configState = EEPROM.read(0); // Check if EEPROM contains config data, aka the first byte is a 1. If not, load the hardcoded defaults
  if (configState == 0) {
    for (int i = 0; i < 8; i++) {
      EEPROM.write(i, configParameters[i]);  // Write initial config parameters to EEPROM
    }
    Serial.println("Initial EEPROM Programming Done");
  } else {
    for (int i = 0; i < 8; i++) {
      configParameters[i] = EEPROM.read(i); // Load current settings from EEPROM
    }
    Serial.println("Settings Restored from EEPROM");  
  }

  if (configParameters[2] == 1) {  // If set to solid on mode, turn seperators on fixed
    digitalWrite(8, HIGH);
    digitalWrite(9, HIGH);
  } else if (configParameters[2] == 0) {  // Keep pins from floating 
    digitalWrite(8, LOW);
    digitalWrite(9, LOW);
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
      if ((currentMillis - previousMillis) >= 1000) {
        digitalWrite(8, !(digitalRead(8)));
        digitalWrite(9, !(digitalRead(9)));
        previousMillis = millis();
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

    delay(10);  // Stability delay
}

// Functions for main code:

// Set tube function
void SetTube(int tube, int num) {
  int a = PORTA >> 4;
  int c = PORTC >> 4;
  int l = PORTL >> 4;
  if (tube % 2 == 0) {
    switch (tube) {
      case 2:
        PORTA = (PORTA - (16 * a)) + (16 * num);
        break;
      case 4:
        PORTC = (PORTC - (16 * c)) + (16 * num);
        break;
      case 6:
        PORTL = (PORTL - (16 * l)) + (16 * num);
        break;
    }
  } else {
    switch (tube) {
      case 1:
        PORTA = (PORTA - (PORTA - (16 * a))) + num;
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
      Serial.print("Bytes in buffer: ");  // Adjust time to recieved value with debugging stuff
      Serial.println(bufferCount);
      if (bufferCount == 15) {
        int yr = (parameters[1] * 1000) + (parameters[2] * 100) + (parameters[3] * 10) + parameters[4];
        int mnth = (parameters[5] * 10) + parameters[6];
        int dy = (parameters[7] * 10) + parameters[8];
        int hr = (parameters[9] * 10) + parameters[10];
        int mn = (parameters[11] * 10) + parameters[12];
        int sc = (parameters[13] * 10) + parameters[14];
        rtc.adjust(DateTime(yr, mnth, dy, hr, mn, sc));
        Serial.println("Date and Time set");
      } else {
        Serial.println("Invalid parameter format");
      }
      break;
    default:
      Serial.println("Invalid Configuration Choice");
      break;
  }
}
