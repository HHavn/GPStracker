// GPS Tracker

// Board: ESP Wrover Module
// Board: LilyGo T-Display (might not be stable)
// Press EN button for download. hold knap nede for boot (ikke RST)
// Briefly press and release the EN (RESET) button while still holding BOOT.
// Release the BOOT button.
//The ESP32 is now in download mode and will respond to the programmer.

// Version control
//#define SOFTWARE_VERSION "1.01"  // First version, basic frame, SD card ok.
//#define SOFTWARE_VERSION "1.02"  // Finalley GPS is working.
//#define SOFTWARE_VERSION "1.03"  // Modem is sending data.
//#define SOFTWARE_VERSION "1.04"  // Message data ready. SD card data.
//#define SOFTWARE_VERSION "1.05"  // Upload to thingspeak
//#define SOFTWARE_VERSION "1.06"  // Receive SMS
//#define SOFTWARE_VERSION "1.07"  // GPS logging to KML file
//#define SOFTWARE_VERSION "1.08"  // SMS cmd structure
//#define SOFTWARE_VERSION "1.09"  // Deep sleep
//#define SOFTWARE_VERSION "1.10"  // Multi SMS read
//#define SOFTWARE_VERSION "1.11"  // Added accumulated travel distance
#define SOFTWARE_VERSION "1.12"  // Log interval degrade according to travel speed

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024  // Set RX buffer to 1Kb

// Libraries for SD card
#include <sys/time.h>
#include "SD.h"
#include <SPI.h>

#include <Arduino.h>
#include <TinyGPS++.h>

#include "utilities.h"
#include <TinyGsmClient.h>

#include "esp_sleep.h"


// Compiler directive. Out-comment to disble debug text output
#define DEBUG
//#define DEBUG2  // GPS and battery output

#ifndef SerialAT
#define SerialAT Serial1
#endif

#ifndef SerialGPS
#define SerialGPS Serial2
#endif

// Modem setup
// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define SMS_TARGET "+4540156239"  //Change the number you want to send sms message


// It depends on the operator whether to set up an APN. If some operators do not set up an APN,
// they will be rejected when registering for the network. You need to ask the local operator for the specific APN.
// APNs from other operators are welcome to submit PRs for filling.
#define NETWORK_APN "internet"  // Lebara
// LEBARA
// Port:8080
// MMC: 238
// MNC: 02

// GPS
#define BOARD_GPS_TX_PIN 21
#define BOARD_GPS_RX_PIN 22
#define BOARD_GPS_PPS_PIN 23
#define BOARD_GPS_WAKEUP_PIN 19

// SPI  SD card reader
#define VSPI_MOSI 15  // VSPI_MOSI
#define VSPI_MISO 2   // VSPI_MISO
#define VSPI_CLK 14   // VSPI_CLK
#define VSPI_CSO 13   // VSPI_CSO

////LILYGO_T_A7670
//#define MODEM_BAUDRATE                      115200
//#define MODEM_DTR_PIN                       25
//#define MODEM_TX_PIN                        26
//#define MODEM_RX_PIN                        27
//// The modem boot pin needs to follow the startup sequence.
//#define BOARD_PWRKEY_PIN                    4
//#define BOARD_ADC_PIN                       35
//// The modem power switch must be set to HIGH for the modem to supply power.
//#define BOARD_POWERON_PIN                   12
//#define MODEM_RING_PIN                      33
//#define MODEM_RESET_PIN                     5
//
//#define BOARD_MISO_PIN                      2
//#define BOARD_MOSI_PIN                      15
//#define BOARD_SCK_PIN                       14
//#define BOARD_SD_CS_PIN                     13
//
//#define BOARD_BAT_ADC_PIN                   35
//#define MODEM_RESET_LEVEL                   HIGH

#define H_TO_uS_FACTOR 3600000000ULL  // Conversion factor for hours to micro seconds
#define SEC_TO_uS_FACTOR 1000000ULL   // Conversion factor for sec to micro seconds
#define MSEC_TO_uS_FACTOR 1000ULL     // Conversion factor for msec to micro seconds

///////////////////////////////////////////////////////////
// Definition of states
enum LOG_STATE { LOG_DISABLED,
                 LOG_ENABLED,
                 LOG_ACTIVE };
LOG_STATE aLogState = LOG_DISABLED;

#define CONFIG_FILE_NAME "/Config.cfg"

#define INPUT_LENGTH 1500
#define LINE_LENGTH 250
#define STRINGS_SIZE 15  // Maximum lines in a SMS message

// Log
#define MIN_DISTANCE_FOR_LOGGING 20    // Meter
#define NO_MOVEMENT_FOR_DEACTIVATE 30  // 5 * 6 ticks a 10 sec = 300 sec = 5 min
#define NO_MOVEMENT_TIME 300000        // mSecond  Stop logging after no movement for this time
#define LOG_DEGRADE_FACTOR 2           // Log interval in sec = speed / this factor
#define LOG_DEGRADE_SPEED 50.0         // Log each aLogInterval below this speed

#define LOW_VOLTAGE_ALERT 3.7      // Battery
#define LOW_VOLTAGE_SHUT_DOWN 3.5  // Battery
#define LOW_INPUT_VOLTAGE 4.0      // Input

// Definition of task interval in seconds
#define SMS_INTERVAL 10000          // msecond
#define BATTERY_INTERVAL 10000      // msecond
#define GPS_LOG_INTERVAL 10000      // msecond
#define DEEP_SLEEP_INTERVAL 120000  // msecond

TinyGPSPlus gps;
TinyGsmClient client(modem, 0);

/////////////////////////////////////////////////////////
// MISC. VARIABLES
float aBatteryVoltage;
float aInputVoltage;

// Operating system task timing
unsigned long aLastRunSMS;  // Time for last SMS read task. Arduino live indication blink. Dummy task, not used
unsigned long aLastRunBattery;
unsigned long aLastRunGpsLog;
unsigned long aLastRunDeepSleep;


// GPS
boolean aGpsIsValid = false;  // gps.location.isValid()
double aGpsLat;               // gps.location.lat()
double aGpsLng;               // gps.location.lng()
double aGpsSpeed;             // gps.speed.kmph();
double aGpsCourse;            // gps.course()
double aGpsAltitude;          // gps.altitude()
int aGpsSattelites;           // gps.sattelites()

TaskHandle_t Task0;  // GPS task 0

String aTimeStamp = "2025------ --:--:--";

String input[INPUT_LENGTH];
String line[LINE_LENGTH];

// SD card
int aSearchIndex;
String aSDdata;
String aSD_CardStatus = "OK  ";
String aActiveGpsLogFile = "";
boolean aSdStatus;
double aCardSize;
double aCardTotal;
double aCardUsed;

// Default data on SD
String aNetworkAPN = NETWORK_APN;
String aSmsTarget = SMS_TARGET;
int aMinDistForLog = MIN_DISTANCE_FOR_LOGGING;  // Radius
int aGpsLogInterval = GPS_LOG_INTERVAL;
int aDeepSleepDuration = 0;
boolean aGpsLogEnabled = false;  // This is input from an enable switch or SD param data
int aDebugSMSlog = 0;            // SMS debug log disabled

//********************************************************************************
// Setup
//********************************************************************************
// The setup  (Core1)
void setup() {
  Serial.begin(115200);
  delay(100);

  //GPS Serial port
  SerialGPS.begin(9600, SERIAL_8N1, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN);
  delay(100);

  SPI.begin(VSPI_CLK, VSPI_MISO, VSPI_MOSI);
  delay(100);

  // Define PIN mode
  // Set ring pin input
  pinMode(MODEM_RING_PIN, INPUT_PULLUP);
  pinMode(MODEM_RESET_PIN, OUTPUT);
  pinMode(BOARD_PWRKEY_PIN, OUTPUT);

  pinMode(BOARD_GPS_WAKEUP_PIN, OUTPUT);

  delay(100);

  gpsWake();  // Activate GPS

  // Turn on DC boost to power on the modem
#ifdef BOARD_POWERON_PIN
  pinMode(BOARD_POWERON_PIN, OUTPUT);
#endif
  delay(100);

  // Set modem baud
  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(100);

  setLocalDefaultTime();  // Init local CPU RTC time. Preset until GPS time is valid

  Serial.print("Program start (Main). Core:");
  Serial.println(xPortGetCoreID());


  // Start the GPS task in core 0
  xTaskCreatePinnedToCore(
    MainTask0,  // Function to implement the task
    "Task0",    // Name of the task
    10000,      // Stack size in words
    NULL,       // Task input parameter
    0,          // Priority of the task
    &Task0,     // Task handle.
    0);         // Core where the task should run


  // Ref issue : https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX/issues/56#issuecomment-1672628977
  //Serial.println("This sketch is only suitable for boards without GPS function inside the modem, such as A7670G");
  //Serial.println("Works only with externally mounted GPS modules. If the purchased board includes a GPS extension module it will work, otherwise this sketch will have no effect");
  //Serial.println("If the purchased modem supports GPS functionality, please use examples / TinyGSM_Net_GNSS");

  // Get the wakeup reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  PrintWakeupReason(wakeup_reason);
  delay(200);

  ModemStart();  // Note this start modem, but modem close down is other commands is not send
  delay(500);
  ReceiveSMSInit();

  //delay(100);
  //InitNetworkAPN();      // Needed for WWW access
  //VariousModemStatusGSM();  // Not needed - just status
  // NetworkStatus();       // Not needed - just status

  delay(200);
  GetSDCardInfo();
  delay(200);
  ReadConfigFile();  // Read the values from SD.
  delay(200);

  ReadBatteryVoltage();

  // Wait for GPS signal
  for (int index = 0; index < 90; index++) {
    if (aGpsIsValid == true) {
      break;
    }
    delay(1000);
  }

  // do {
  //   // just wait
  // } while (millis() < 60000);  // Virker med dette delay

  // Initial read SMS before sending the first SMS
  ReadSMS();  //Virker med denne

  if (aGpsIsValid == true) {
    if (aTimeStamp.indexOf("------") > 0) {
      Println("\nSend SMS, Boot OK. GPS not ready");
      if (aDebugSMSlog >= 2) {
        SendSMS("LilyGo ESP32 Boot OK. GPS not ready\n", aSmsTarget);
      }
    } else {
      Println("\nSend SMS, Boot and GPS OK");
      if (aDebugSMSlog >= 2) {
        String smsText1 = "LilyGo ESP32 boot OK\n";
        String smsText2 = aTimeStamp;
        String smsText = smsText1 + smsText2;
        SendSMS(smsText, aSmsTarget);
      }
    }
  } else {
    Println("");
    Println("Send SMS, Boot OK, GPS not ready");
    String smsText = "LilyGo ESP32 boot. GPS not ready!\n";
    smsText += aTimeStamp;
    SendSMS(smsText, aSmsTarget);
  }
  delay(200);

  SdLogger("System Boot");
  delay(200);

  // Initialize task timing
  aLastRunSMS = millis();        // task init
  aLastRunBattery = millis();    // task init
  aLastRunGpsLog = millis();     // task init
  aLastRunDeepSleep = millis();  // task init
}


//********************************************************************************
// Loop Core 1
//********************************************************************************
void loop() {
  //static unsigned long SMSdelay = 0;

  // Battery task.
  if (millis() - aLastRunBattery >= BATTERY_INTERVAL) {
    aLastRunBattery = millis();  // The current time in millisecond since boot
    ReadBatteryVoltage();
  }

  // SMS read handling task.
  if (millis() - aLastRunSMS >= SMS_INTERVAL) {
    aLastRunSMS = millis();  // The current time in millisecond since boot
    ReadSMS();               // Call this subroutine each SMS_INTERVAL
  }

  // GPS log task.
  if (millis() - aLastRunGpsLog >= aGpsLogInterval) {
    aLastRunGpsLog = millis();  // The current time in millisecond since boot
    GpsLog();
  }

  // Deep sleeptask.
  if (millis() - aLastRunDeepSleep >= DEEP_SLEEP_INTERVAL) {
    aLastRunDeepSleep = millis();  // The current time in millisecond since boot
    DeepSleepCheck();
  }


  // This is for testing AT commands via serial monitor
  //  if (SerialAT.available()) {
  //    Serial.write(SerialAT.read());  // Read data from modem and display
  //  }

  //  if (Serial.available()) {
  //    SerialAT.write(Serial.read());  // Read data from keyboard and send to modem
  //  }

  //  delay(1);
}


//----------------------------------------------------------------------
// print
void Print(String input) {
#ifdef DEBUG
  Serial.print(input);
#endif
}


//----------------------------------------------------------------------
// print
void Println(String input) {
#ifdef DEBUG
  Serial.println(input);
#endif
}


//----------------------------------------------------------------------
// Control deep sleep
void DeepSleepCheck() {
  Println("DeepSleep() check");

  if ((aLogState != LOG_ACTIVE) && (aDeepSleepDuration > 0)) {
    ActivateDeepSleep(aDeepSleepDuration);
  }
}


//----------------------------------------------------------------------
void ActivateDeepSleep(int deepSleepDuration) {
  Println("Deep Sleep activated\n" + aTimeStamp);

  if (aDebugSMSlog >= 2) {
    String smsText = "Deep Sleep activated\n";
    smsText += aTimeStamp;
    String lin3 = "\nBattery: " + String(aBatteryVoltage, 2) + " V";
    smsText += lin3;
    SendSMS(smsText, aSmsTarget);
  }

  delay(2000);

  // Power down GPS
  gpsSleep();
  delay(1000);

  // Power down modem
  ModemStop();

  delay(3000);

  // Go to sleep
  Serial.flush();  // Ensure all serial data is sent
  delay(100);

#define TRIM_TIME_SEC 28  // seconds

  // Set wakeup timer
  esp_sleep_enable_timer_wakeup((deepSleepDuration * H_TO_uS_FACTOR) - (DEEP_SLEEP_INTERVAL * MSEC_TO_uS_FACTOR) + (TRIM_TIME_SEC * SEC_TO_uS_FACTOR));
  //esp_sleep_enable_timer_wakeup(60 * 1000000);  // 60 sec     TEMP for test

  delay(200);

  esp_deep_sleep_start();  // All output will be high set.
                           // If your application needs to keep some pins at a certain level during deep sleep (for example, keep a relay energized or keep an LED on), you should:
                           // Use RTC GPIO pins (e.g., GPIOs 0, 2, 4, 12-15, 25-27, 32-39 depending on your ESP32 model)
                           // Configure their output states just before sleep
                           // Use the RTC peripheral to hold the output state during deep sleep
}


//----------------------------------------------------------------------
void PrintWakeupReason(esp_sleep_wakeup_cause_t wakeup_reason) {
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      Println("Wakeup cause: Undefined - Normal Boot!");
      break;

    case ESP_SLEEP_WAKEUP_EXT0:
      Println("Wakeup caused by external signal (EXT0)");
      break;

    case ESP_SLEEP_WAKEUP_EXT1:
      Println("Wakeup caused by external signal (EXT1)");
      break;

    case ESP_SLEEP_WAKEUP_TIMER:
      Println("Wakeup caused by timer");
      break;

    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Println("Wakeup caused by touchpad");
      break;

    case ESP_SLEEP_WAKEUP_ULP:
      Println("Wakeup caused by ULP program");
      break;

    default:
      Println("Wakeup cause: Unknown");
      break;
  }
}



//----------------------------------------------------------------------
// Start at movement.
// Stop at standstill for more than 5 minutes.
void GpsLog() {
  static boolean enabledebugOutput = true;
  static float logLat;
  static float logLng;
  static float lastLat;
  static float lastLng;
  static float totalDistance;
  float distance;
  static unsigned long timeStampForNoMovement = 0;
  static unsigned long timeStampForLastGpsValid = 0;
  static unsigned long timeForLastLog = 0;
  static unsigned long timeForLogStart;
  static int gpsInvalidCount = 0;  // Count the number of times GPS is invalid in a row
  String textToSend;

  // State control, normal function
  switch (aLogState) {
    case LOG_DISABLED:
      if ((aGpsLogEnabled == true) && (aGpsIsValid == true)) {  // From switch/SMS input
        // Check if position is stable
        distance = TinyGPSPlus::distanceBetween(lastLat, lastLng, aGpsLat, aGpsLng);
        if (distance < 2.0) {
          aLogState = LOG_ENABLED;
          Println("Logging enabled.");
          // Set the initial stable position
          logLat = aGpsLat;
          logLng = aGpsLng;

          if ((aDebugSMSlog >= 1) && (enabledebugOutput == true)) {
            textToSend = "Logging enabled\n";
            textToSend += aTimeStamp;
            textToSend += "\nBattery: ";
            textToSend += String(aBatteryVoltage, 2);
            textToSend += " V";
            SendSMS(textToSend, aSmsTarget);
          }
        }
      }
      break;

    case LOG_ENABLED:
      if (aGpsLogEnabled == true) {  // From switch or SMS input
        if (aGpsIsValid == true) {   // Only start logging when GPS is valid
          // If movement then change state
          distance = TinyGPSPlus::distanceBetween(logLat, logLng, aGpsLat, aGpsLng);

          Print("distance: ");
          Println(String(distance));

          if (distance > aMinDistForLog) {  // Radius
            aLogState = LOG_ACTIVE;
            Println("Movement detected, init log");

            textToSend = "Logging started, distance:  ";
            textToSend += String(distance);
            SdLogger(textToSend);  // Tactical log

            // There is movement, do the position logging
            SdGpsLogger(logLat, logLng, false);  // First log, make header and do first log
            timeForLastLog = millis();

            if (aDebugSMSlog >= 1) {
              textToSend = "Logging started\n";
              textToSend += aTimeStamp;
              textToSend += "\nDistance: ";
              textToSend += String(distance);
              SendSMS(textToSend, aSmsTarget);
            }

            timeStampForNoMovement = 0;
            totalDistance = distance;  // Initial distance difference
            timeForLogStart = millis();
          }
        }  // aGpsValid
      } else {
        enabledebugOutput = true;
        aLogState = LOG_DISABLED;
        enabledebugOutput = true;
        Println("Logging disabled.");
        if (aDebugSMSlog >= 1) {
          textToSend = "Logging disabled\n";
          textToSend += aTimeStamp;
          SendSMS(textToSend, aSmsTarget);
        }
      }
      break;

    case LOG_ACTIVE:
      if (aGpsLogEnabled == true) {  // From switch/SMS input
        if (aGpsIsValid == true) {   // Only log is GPS is active
          timeStampForLastGpsValid = millis();
          // loggin active
          // Only do logging if distance traveled is larger than NN
          distance = TinyGPSPlus::distanceBetween(logLat, logLng, aGpsLat, aGpsLng);

          Print("distance: ");
          Println(String(distance));

          if (distance > aMinDistForLog) {  // Radius
            // Degrade logging rate if speed > LOG_DEGRADE_SPEED.  100 kmt will log each 50 second
            if ((aGpsSpeed < LOG_DEGRADE_SPEED) || (((millis() - timeForLastLog) / 1000) > (aGpsSpeed /LOG_DEGRADE_FACTOR))) {

              // Do a real normal GPS logging
              Serial.println("Movement detected, do logging");
              // There is movement, do the logging
              SdGpsLogger(aGpsLat, aGpsLng, false);
              timeForLastLog = millis();

              // Set the logged position as new reference
              logLat = aGpsLat;
              logLng = aGpsLng;

              timeStampForNoMovement = 0;  // Reset - there is movement

              totalDistance += distance;
            }
          } else {
            Serial.println("No movement detected");
            // Logging is active, but no movement detected
            if (timeStampForNoMovement == 0) {
              timeStampForNoMovement = millis();  // Set first time no movement is detected
            }

            Print("millis() - timeStampForNoMovement: ");
            String time = String((millis() - timeStampForNoMovement) / 1000);
            Println(time);

            if (millis() - timeStampForNoMovement > NO_MOVEMENT_TIME) {
              Serial.println("No movement detected for NO_MOVEMENT_TIME. Stop active logging");

              // Stop any active loggin
              SdGpsLogger(aGpsLat, aGpsLng, true);  // Close file

              totalDistance += distance;

              enabledebugOutput = false;
              aLogState = LOG_DISABLED;

              textToSend = "Logging stopped, no movement\n";
              textToSend += aTimeStamp;
              textToSend += "\nDistance: ";
              textToSend += String(distance);
              textToSend += "\nTotal Distance: ";
              textToSend += String(totalDistance);
              textToSend += "\nTime: ";
              textToSend += time;

              if (aDebugSMSlog >= 1) {
                SendSMS(textToSend, aSmsTarget);
              }

              textToSend = "Logging stopped, no movement. Total distance: ";
              textToSend += String(totalDistance);
              textToSend += "  Log time: ";

              unsigned long totalTimeSec = (millis() - timeForLogStart) / 1000;
              int timeMin = totalTimeSec / 60;
              int timeSec = totalTimeSec % 60;

              textToSend += String(timeMin);
              textToSend += ":";
              if (timeSec < 10) {
                textToSend += "0";  // leading zero
              }
              textToSend += String(timeSec);

              SdLogger(textToSend);
            }
          }     // distance >
        }       // GpsIsValid
      } else {  // // GPS logging is not enabled
        enabledebugOutput = true;
        aLogState = LOG_DISABLED;
        SdLogger("Logging stopped, disabled");
        Println("Logging stopped and disabled");
        if (aDebugSMSlog >= 1) {
          textToSend = "Logging stopped and disabled\n";
          textToSend += aTimeStamp;
          SendSMS(textToSend, aSmsTarget);
        }
        // Stop any active loggin
        SdGpsLogger(aGpsLat, aGpsLng, true);  // Close file
      }

      // No GPS for one minute - stop logging
      if (millis() - timeStampForLastGpsValid > 60000) {
        enabledebugOutput = true;
        aLogState = LOG_DISABLED;
        SdLogger("Logging stopped, no GPS for one minute");
        Println("Logging stopped and disabled. No GPS.");
        if (aDebugSMSlog >= 1) {
          textToSend = "Logging stopped and disabled\n";
          textToSend += aTimeStamp;
          SendSMS(textToSend, aSmsTarget);
        }
        // Stop any active loggin
        SdGpsLogger(aGpsLat, aGpsLng, true);  // Close file
      }

      break;
  }  // switch


  lastLat = aGpsLat;  // Used in LOG_DISABLED mode
  lastLng = aGpsLng;

  Print("GpsLog(). next aLogState: ");
  Println(String(aLogState));
}




//----------------------------------------------------------------------
void ReadBatteryVoltage() {
  static boolean alertActivated = false;

  aBatteryVoltage = (analogReadMilliVolts(BOARD_BAT_ADC_PIN) * 2.0) / 1000.0;
  // The hardware voltage divider resistor is half of the actual voltage,
  // multiply it by 2 to get the true voltage

  aInputVoltage = (analogReadMilliVolts(BOARD_SOLAR_ADC_PIN) * 2.0) / 1000.0;
  // The hardware voltage divider resistor is half of the actual voltage,
  // multiply it by 2 to get the true voltage

#ifdef DEBUG2
  Serial.print("Battery Voltage: ");
  Serial.println(aBatteryVoltage, 2);
  Serial.print("Charge Voltage: ");
  Serial.println(aInputVoltage, 2);
#endif


  if ((aBatteryVoltage <= LOW_VOLTAGE_ALERT) && (alertActivated == false)) {
    String msg = "Low battery alert: ";
    msg += String(aBatteryVoltage, 2);
    msg += "\n";
    msg += aTimeStamp;
    SendSMS(msg, aSmsTarget);
    alertActivated = true;  // Only send one time
  }

  if ((aBatteryVoltage <= LOW_VOLTAGE_SHUT_DOWN) && (aInputVoltage < LOW_INPUT_VOLTAGE)) {
    String msg = "Low battery. System shutdown!: ";
    msg += String(aBatteryVoltage, 2);
    msg += "\n";
    msg += aTimeStamp;
    SendSMS(msg, aSmsTarget);

    ActivateDeepSleep(1000);  // 1000 hours
  }

  if ((aBatteryVoltage > (LOW_VOLTAGE_ALERT + 0.2)) && (alertActivated == true)) {
    alertActivated = false;  // Re-arm
  }
}



//----------------------------------------------------------------------
// Map input to output:  input, inMin, inMax, outMin, OutMax
// This will change the input range to the wanted output range.
// Make sure that (inMax - inMin) + outMin is not zero.
float MapFloat(float input, float inMin, float inMax, float outMin, float outMax) {
  float result;
  float divisor = inMax - inMin + outMin;

  if (divisor == 0.0) {
    divisor = 0.00001;
    result = ((input - inMin) * (outMax - outMin)) / divisor;
  } else {
    result = (input - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
  }

  return result;
}



//----------------------------------------------------------------------
// Modem stop
void ModemStop() {
  modem.poweroff();
  delay(1000);
  digitalWrite(BOARD_POWERON_PIN, LOW);  // power off modem
}



//----------------------------------------------------------------------
// Modem start
void ModemStart() {
  // Turn on DC boost to power on the modem
#ifdef BOARD_POWERON_PIN
  digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

  // Set modem reset pin ,reset modem
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  delay(100);
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  delay(2600);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

  // Turn on modem
  Serial.println("Start modem...");
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  delay(1000);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);

  delay(3000);

  // Wait for modem
  while (!modem.testAT()) {
    delay(10);
  }


  delay(3000);

  Println("Wait for SIM ready");
  modem.sendAT("+CPIN?");
  modem.waitResponse();
  delay(1000);

  Println("Wait for registration");
  modem.sendAT("+CEREG?");
  modem.waitResponse();
  delay(1000);


  // Wait PB DONE
  //Println("Wait SMS Done.");

  // if (!modem.waitResponse(100000UL, "SMS DONE")) {
  //   Serial.println("Can't wait from sms register ....");
  //   return;
  // }

  // Check if SIM card is online
  SimStatus sim = SIM_ERROR;
  while (sim != SIM_READY) {
    sim = modem.getSimStatus();
    switch (sim) {
      case SIM_READY:
        Println("SIM card online");
        break;
      case SIM_LOCKED:
        Serial.println("The SIM card is locked. Please unlock the SIM card first.");
        // const char *SIMCARD_PIN_CODE = "123456";
        // modem.simUnlock(SIMCARD_PIN_CODE);
        break;
      default:
        break;
    }
    delay(1000);
  }
}


//----------------------------------------------------------------------
// Network APN
void InitNetworkAPN() {

#ifdef DEBUG
  Serial.printf("Set network apn : %s\n", aNetworkAPN);
#endif
  modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), aNetworkAPN, "\"");
  if (modem.waitResponse() != 1) {
    Serial.println("Set network apn error !");
  }
}



//----------------------------------------------------------------------
// Modem status
void VariousModemStatusGSM() {
  // Get model info
  Println(" Get model info");
  modem.sendAT("+SIMCOMATI");
  modem.waitResponse();

  delay(500);

  String ccid = modem.getSimCCID();
  Print("CCID: ");
  Println(ccid);

  delay(500);

  String imei = modem.getIMEI();
  Print("IMEI: ");
  Println(imei);

  delay(500);

  String imsi = modem.getIMSI();
  Print("IMSI: ");
  Println(imsi);

  delay(500);

  String cop = modem.getOperator();
  Print("Operator: ");
  Serial.println(cop);

  delay(500);

  int csq = modem.getSignalQuality();
  Print("Signal quality: ");
  Println(String(csq));

  delay(500);
}



//----------------------------------------------------------------------
// Modem status
void VariousModemStatusNet() {

  String ipAddress = modem.getLocalIP();
  Print("Network IP:");
  Println(ipAddress);

  delay(500);

  bool res = modem.isGprsConnected();
  Print("GPRS status: ");
  Println(String(res));

  delay(500);

  IPAddress local = modem.localIP();
  Print("Local IP: ");
  Println(String(local));

  delay(500);
}


//----------------------------------------------------------------------
// SMS
void ReceiveSMSInit() {
  Println("Init SMS receive");
  Println("Configuring TEXT mode");

  modem.sendAT("+CMGF=1");  // Text mode setup
  modem.waitResponse(1000);

  //Println("Decides how newly arrived SMS messages should be handled");

  modem.sendAT("+CNMI=2,2,0,0,0");  // Ingen notifikationer = AT+CNMI=0,0,0,0,0
                                    // modem.sendAT("+CNMI=2,1,0,0,0");  // AT+CNMI=2,2,0,0,0  Oprindelig
                                    // AT+CNMI=<mode>,<mt>,<bm>,<ds>,<bfr>
                                    // mt=1  Der modtages +CMTI: "SM",6.    SMS modtaget. 6=index. SM=SIM ram.
                                    // mt=2  Hele SMS modtages via serial
                                    // Når en ny SMS kommer, sender modem’et en +CMT: linje med hele beskeden direkte til serielporten.
                                    // (Hvis du havde sat 2, ville du kun få besked om, at der er en ny SMS, og så skulle du selv hente den med AT+CMGR.)
  modem.waitResponse(1000);
}


//----------------------------------------------------------------------
// Read SMS
void ReadSMS() {
  String totalSMS = "";
  String strings[STRINGS_SIZE];  // Max amount of strings anticipated

  //---------------------Hugo code-----------------------------------
  if (SerialAT.available() > 0) {
    Print("SMS received. SerialAT buffer: ");
    Println(String(SerialAT.available()));
  }

  //Read the SMS data from the modem
  // +CMT: "+4540156239","","25/08/30,10:37:40+8"
  // Ffffffg

  // +CMT: "+4540156239","","25/08/30,10:37:42+8"
  // Fcccg

  // +CMT: "+4540156239","","25/08/30,10:38:49+8"
  // Ffffxf


  while (SerialAT.available() > 0) {
    char c = SerialAT.read();
    totalSMS += c;  // Append character to the line
  }                 // SerialAT.available


  if (totalSMS.length() > 0) {
    // Split SMS in lines
    int noOfLines = split('\n', totalSMS, strings);

    // Print total collection of SMS
    for (int index = 0; index <= noOfLines; ++index) {
      Print("Line: ");
      Print(String(index));
      Print(" : ");
      Println(strings[index]);
    }

    // Loop through all SMS lines
    for (int index = 0; index <= noOfLines; ++index) {
      String header = strings[index];
      header.trim();
      Print("Header line search: ");
      Println(strings[index]);
      if (header.startsWith("+CMT:")) {
        Println("Header found");
        String line1 = "";
        String line2 = "";

        if (index < noOfLines) {
          line1 = strings[index + 1];
        }

        if (index + 1 < noOfLines) {
          line2 = strings[index + 2];
        }

        HandleSMS(header, line1, line2);
      }  // (line.startsWith("+CMT:")) {
    }    // NoOfLines
  }      // length
}



//--------------------------------------------------------------------
// Input +CMT header line and one or two lines more.
void HandleSMS(String header, String line1, String line2) {
  boolean cmdFound = false;

  // Extract header
  // 01234567890123456789012345678901234567890123456
  // +CMT: "+4540156239","","24/10/03,22:22:53+8"
  String sender = header.substring(7, 18);
  String receiveTime = header.substring(24, 41);
  Print("header: ");
  Println(header);
  Print("Sender: ");
  Println(sender);
  Print("Receive time: ");
  Println(receiveTime);

  // Check if correct sender
  if (sender.indexOf(aSmsTarget) > -1) {

    // first line of the message
    Print("Line1: ");
    Println(line1);

    line1.toLowerCase();

    // Check if command received
    //-------------------------------------------------------------------------
    if ((line1.indexOf("getstat") > -1) || (line1.indexOf("gst") > -1)) {
      GetStatus(line1, sender);
      cmdFound = true;
    }

    //-------------------------------------------------------------------------
    if ((line1.indexOf("getpos") > -1) || (line1.indexOf("gpo") > -1)) {
      GetPosition(line1, sender);
      cmdFound = true;
    }

    //-------------------------------------------------------------------------
    if ((line1.indexOf("getparam") > -1) || (line1.indexOf("gpa") > -1)) {
      GetParam(line1, sender);
      cmdFound = true;
    }

    //-------------------------------------------------------------------------
    if ((line1.indexOf("setparam") > -1) || (line1.indexOf("spa") > -1)) {
      // Use second line of the message
      SetParam(line1, line2, sender);
      cmdFound = true;
    }


    if (cmdFound == true) {
      // Postphone deep sleep
      aLastRunDeepSleep = millis();  // The current time in millisecond since boot
    }

    if (cmdFound == false) {
      String lin3 = "Unknown command!\n";
      String lin4 = "Legal cmd: \n";
      String lin5 = "GetStat\n";
      String lin6 = "GetPos\n";
      String lin7 = "GetParam\n";
      String lin8 = "SetParam\n";
      String SMSText = lin3 + lin4 + lin5 + lin6 + lin7 + lin8;
      SendSMS(SMSText, sender);
    }
  } else {  // check sender
            // Illegal sender
    String lin1 = "Unknown sender!\nYou are not allowed to do anything!";

    String SMSText = lin1;
    SendSMS(SMSText, sender);
  }
}


//--------------------------------------------------------------------
void GetStatus(String line1, String sender) {
  String SMSText = "";
  String logState;

  Serial.println("getstatus received");

  //number speed, altitude, position, last logfile name,
  // aLogState  enum LOG_STATE {LOG_DISABLED, LOG_ENABLED, LOG_ACTIVE};

  if (aLogState == LOG_DISABLED) {
    logState = "Log disabled";
  }

  if (aLogState == LOG_ENABLED) {
    logState = "Log enabled";
  }

  if (aLogState == LOG_ACTIVE) {
    logState = "Log active";
  }

  String sdStat = "OK";
  if (aSdStatus = false) {
    sdStat = "ERROR";
  }

  int csq = modem.getSignalQuality();
  GetSDCardInfo();

  String lin2 = "Cmd: " + line1 + "\n";
  String lin3 = "Battery: " + String(aBatteryVoltage, 2) + " V\n";
  String lin4 = "Charge Input: " + String(aInputVoltage, 2) + " V\n";
  String lin5 = "Log state: " + logState + "\n";
  String lin6 = "GSM signal Q: " + String(csq) + "\n";
  String lin7 = "SD status: " + sdStat + "\n";
  String lin8 = "SD size: " + String(aCardSize, 0) + " MB\n";
  String lin9 = "SD used: " + String(aCardUsed, 2) + " MB\n";
  SMSText = lin2 + lin3 + lin4 + lin5 + lin6 + lin7 + lin8 + lin9;

  Print("SMS size: ");
  Println(String(SMSText.length()));

  SendSMS(SMSText, sender);
}



//--------------------------------------------------------------------
void GetPosition(String line1, String sender) {
  String SMSText = "";

  Serial.println("getPos received");


  String lin1 = "Cmd: " + line1 + "\n";
  String lin2 = aTimeStamp + "\n";
  String lin3 = "Lat: " + String(aGpsLat, 6) + "\n";
  String lin4 = "Lng : " + String(aGpsLng, 6) + "\n";
  String lin5 = "Speed: " + String(aGpsSpeed, 1) + "\n";
  String lin6 = "Altitude: " + String(aGpsAltitude, 0) + "\n";
  String lin7 = "Sattelites: " + String(aGpsSattelites) + "\n";

  if (aGpsIsValid == true) {
    SMSText = lin1 + lin2 + lin3 + lin4 + lin5 + lin6 + lin7;
  } else {
    lin3 = "No GPS data!";
    SMSText = lin1 + lin2 + lin3;
  }

  Print("SMS size: ");
  Println(String(SMSText.length()));

  SendSMS(SMSText, sender);
}


//--------------------------------------------------------------------
void GetParam(String line1, String sender) {
  String SMSText = "";

  Serial.println("getparam received");
  ReadConfigFile();

  String lin2 = "Cmd: " + line1 + "\n";
  String lin3 = GetRealSDdata(aSDdata);
  SMSText = lin2 + lin3;

  Print("SMS size: ");
  Println(String(SMSText.length()));

  SendSMS(SMSText, sender);
}



//--------------------------------------------------------------------
void SetParam(String line1, String line2, String sender) {
  String strings[STRINGS_SIZE];  // Max amount of strings anticipated
  String SMSText = "";
  int noOfStrings;
  int paramInt;
  boolean paramCorrected = false;
  boolean paramOK = true;
  String SDdataNew = "";
  String cmd = "";
  String item = "";

  Serial.println("setparam received");

  // Extract second line of the message
  if (line2.isEmpty() == false) {
    Print("Line2: ");
    Println(line2);

    noOfStrings = split('=', line2, strings);

    if (noOfStrings >= 2) {
      cmd = strings[0];
      item = strings[1];

      cmd.trim();
      cmd.toLowerCase();
      item.trim();
      item.toLowerCase();

      Serial.print("cmd: ");
      Serial.println(cmd);
      Serial.print("item: ");
      Serial.println(item);

      // Check the cmd/item
      if ((cmd.startsWith("radius") == true) || (cmd.startsWith("loginterval") == true) || (cmd.startsWith("deepsleep") == true) || (cmd.startsWith("debug") == true)) {
        // Test numeric value
        if (IsDigits(item) == false) {
          paramOK = false;
        } else {
          paramInt = item.toInt();
          if (cmd.startsWith("radius") == true) {
            if ((paramInt < MIN_DISTANCE_FOR_LOGGING) || (paramInt > 100)) {
              paramOK = false;
            }
          }

          if (cmd.startsWith("loginterval") == true) {
            if ((paramInt < 1) || (paramInt > 200)) {
              paramOK = false;
            }
          }

          if (cmd.startsWith("deepsleep") == true) {
            if ((paramInt < 0) || (paramInt > 48)) {
              paramOK = false;
            }
          }

          if (cmd.startsWith("debug") == true) {
            if ((paramInt < 0) || (paramInt > 2)) {
              paramOK = false;
            }
          }
        }
      }

      if (cmd.startsWith("logenable") == true) {
        // Test true/false value
        if ((item.startsWith("true") == false) && (item.startsWith("false") == false)) {
          paramOK = false;
        }
      }

      if (paramOK == true) {

        ReadConfigFile();  // Read config file to aSDdata;

        // Split aSDdata in lines
        noOfStrings = split('\n', aSDdata, strings);

        // Print data
        for (int index = 0; index <= noOfStrings; ++index) {
          Serial.print("Line: ");
          Serial.print(index);
          Serial.print(" : ");
          Serial.println(strings[index]);
        }

        // Run through all lines in aSDdata
        for (int index = 0; index <= noOfStrings; ++index) {
          String line = strings[index];
          //Serial.print("Line: ");
          //Serial.println(line);

          // Skip comment lines
          if (line.startsWith("#") == false) {
            // Real line, search setting command
            if (line.startsWith(cmd) == true) {
              // Correct the data
              Serial.println("command found in SD");
              // The new line
              line = cmd + "=" + item + "\r";
              Serial.print("New line: ");
              Serial.println(line);
              paramCorrected = true;
            }
          }

          if (line.isEmpty() == false) {  // Skip empty lines
            SDdataNew += line + "\n";
          }
        }  // for...

        if (paramCorrected == true) {
          // Write config file from aSDdataNew
          Serial.println("New data: ");
          Serial.println(SDdataNew);
          WriteConfigFile(SDdataNew);
        }

      }  // ParamOK
    }    // noOfStrings == 2
  }      // line2.isEmpty


  String lin2 = "Cmd: " + line1 + "\n";
  String lin3 = "Setting: " + line2 + "\n";
  String lin4;
  if (paramCorrected == true) {
    lin4 = "Accepted\n";
  } else {
    lin4 = "Rejected\n";
  }

  SMSText = lin2 + lin3 + lin4;

  SendSMS(SMSText, sender);

  if (paramCorrected == true) {
    ReadConfigFile();  // This is done to set the local variables.
  }
}



//--------------------------------------------------------------------
// Remove the comment lines
String GetRealSDdata(String aSDdata) {
  String result = "";
  String line = "";

  for (int index = 0; index < aSDdata.length(); index++) {
    char nextChar = aSDdata.charAt(index);
    if (nextChar == '\r') {
      // ignore
    } else if (nextChar == '\n') {
      // we have the next line
      line += String(nextChar);

      // Skip comment lines
      if (line.startsWith("#") == false) {
        result += line;
      }

      line = "";
    } else {
      line += String(nextChar);
    }
  }

  return result;
}


//----------------------------------------------------------------------
// Send SMS
void SendSMS(String input, String receiver) {
  //aSmsCount++;

  Print("Send SMS message to  ");
  Println(receiver);

  bool res = modem.sendSMS(receiver, input);
  Print("Send sms message ");
  Println(res ? "OK" : "fail");
}


//********************************************************************************
// SD-card
//********************************************************************************
void GetSDCardInfo() {
  boolean SDstatus = true;

  Println("Get SD card information");

  //  boolean SDstatus = SD.begin(VSPI_CSO);  // Mount SD
  if (SD.begin(VSPI_CSO) == false) {
    Println("Card Mount Failed");
    SDstatus = false;
  }

  if (SDstatus == true) {
    int cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
      return;
    }


    Print("SD Card Type: ");
    if (cardType == CARD_MMC) {
      Println("MMC");
    } else if (cardType == CARD_SD) {
      Println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Println("SDHC");
    } else {
      Println("UNKNOWN");
    }


    aCardSize = SD.cardSize() / (1024 * 1024);
    aCardTotal = SD.totalBytes() / (1024 * 1024);
    aCardUsed = SD.usedBytes() / (1024.0 * 1024.0);

    Print("SD Card Size: ");
    Print(String(aCardSize));
    Println(" MB");
    Print("Total space: ");
    Print(String(aCardTotal));
    Println(" MB");
    Print("Used space: ");
    Print(String(aCardUsed));
    Println(" MB");
  }

  aSdStatus = SDstatus;

  SD.end();  // Unmount
}


//--------------------------------------------------------------------
// Read config file: Config.cnf
void ReadConfigFile() {
  boolean SDstatus = true;
  String inputFileName = CONFIG_FILE_NAME;
  File inputFile;

  Println("ReadConfigFile called");

  aSDdata = "";

  // Initialize SD card
  // SPI.begin(VSPI_CLK, VSPI_MISO, VSPI_MOSI);
  if (SD.begin(VSPI_CSO) == false) {
    Println("Card Mount Failed");
    SDstatus = false;
  }

  if (SDstatus == true) {
    Print("Input Filename : ");
    Println(inputFileName);


    if (SD.exists(inputFileName)) {
      Println("Input File open OK");

      inputFile = SD.open(inputFileName);
      if (inputFile == true) {
        Println("Read from input file : ");

        // Do the copy
        while (inputFile.available()) {
          char inputChar = inputFile.read();  // Read one ch from file
          aSDdata += inputChar;
        }
      } else {
        SDstatus = false;
        Println("Failed to open file for reading");
      }

      inputFile.close();
    } else {
      SDstatus = false;
      Println("Config.cfg file do not exist");

      CreateConfigFile();  // Create default file
    }
  }  // SDstatus == true


  if (SDstatus == true) {
    aSD_CardStatus = "OK    ";
    Println(aSDdata);

    //SetUnit();
    SetNetworkAPN();
    delay(20);
    SetTelephone();
    delay(20);
    SetLogRadius();
    delay(20);
    SetLogInterval();
    delay(20);
    SetDeepSleep();
    delay(20);
    SetLogEnable();
    delay(20);
    SetDebug();
  } else {
    aSD_CardStatus = "ERROR4";
  }

  SD.end();  // Unmount
}

//--------------------------------------------------------------------
void SetNetworkAPN() {
  String apn;

  aSearchIndex = 0;  // Start search from beginning

  apn = GetNext("NetworkAPN");
  apn.trim();

  if (apn.length() > 3) {
    aNetworkAPN = apn;
  } else {
    Serial.print("ILLEGAL NetworkAPN: ");
    Serial.println(apn);
  }

  Print("NetworkAPN: ");
  Println(aNetworkAPN);
}


//--------------------------------------------------------------------
void SetTelephone() {
  String input;
  String inputSub;

  aSearchIndex = 0;  // Start search from beginning

  input = GetNext("tlf");
  input.trim();
  inputSub = input.substring(1);

  if ((input.length() > 0) && (IsDigits(inputSub) == true)) {
    aSmsTarget = input;
  } else {
    Serial.print("ILLEGAL telephone number: ");
    Serial.println(input);
  }

  Print("Telephone number: ");
  Println(aSmsTarget);
}


//--------------------------------------------------------------------
void SetLogRadius() {
  String input;

  aSearchIndex = 0;  // Start search from beginning

  input = GetNext("radius");
  input.trim();

  if ((input.length() > 0) && (IsDigits(input) == true)) {
    aMinDistForLog = input.toInt();
  } else {
    Serial.print("ILLEGAL radius: ");
    Serial.println(input);
  }

  Print("Min log radius: ");
  Println(String(aMinDistForLog));
}


//--------------------------------------------------------------------
void SetLogInterval() {
  String input;

  aSearchIndex = 0;  // Start search from beginning

  input = GetNext("loginterval");
  input.trim();

  if ((input.length() > 0) && (IsDigits(input) == true)) {
    aGpsLogInterval = input.toInt() * 1000;
  } else {
    Serial.print("ILLEGAL loginterval: ");
    Serial.println(input);
  }

  Print("GPS log interval in seconds: ");
  Println(String(aGpsLogInterval / 1000));
}


//--------------------------------------------------------------------
void SetDeepSleep() {
  String input;

  aSearchIndex = 0;  // Start search from beginning

  input = GetNext("deepsleep");
  input.trim();

  if ((input.length() > 0) && (IsDigits(input) == true)) {
    aDeepSleepDuration = input.toInt();
  } else {
    Serial.print("ILLEGAL deepsleep: ");
    Serial.println(input);
  }

  Print("Deep sleep duration in hours: ");
  Println(String(aDeepSleepDuration));
}


//--------------------------------------------------------------------
void SetLogEnable() {
  String input;

  aSearchIndex = 0;  // Start search from beginning

  input = GetNext("logenable");
  input.trim();

  if ((input.length() >= 4) || (input.length() <= 5)) {
    if (input.indexOf("true") > -1) {
      aGpsLogEnabled = true;
    } else {
      aGpsLogEnabled = false;
    }
  } else {
    Serial.print("ILLEGAL logenable: ");
    Serial.println(input);
  }

  Print("GpsLogEnabled: ");
  Println(String(aGpsLogEnabled));
}


//--------------------------------------------------------------------
void SetDebug() {
  String input;

  aSearchIndex = 0;  // Start search from beginning

  input = GetNext("debug");
  input.trim();

  if ((input.length() > 0) && (IsDigits(input) == true)) {
    aDebugSMSlog = input.toInt();
  } else {
    Serial.print("ILLEGAL logenable: ");
    Serial.println(input);
  }

  Print("aDebugSMSlog: ");
  Println(String(aDebugSMSlog));
}



//--------------------------------------------------------------------
boolean IsDigits(String input) {
  boolean result = true;

  for (int index = 0; index < input.length(); index++) {
    if (isdigit(input.charAt(index)) != true) {
      result = false;
      break;
    }
  }

  return result;
}



//--------------------------------------------------------------------
// Ignore comment lines starting with #
String GetNext(String item) {
  String result = "";
  String line;
  int index;

  do {
    line = GetNextLine();
    if (line.length() > 0) {
      // Skip comment lines
      if (line.startsWith("#") == false) {

        // does the line contain the item.
        index = line.indexOf(item);

        if (index > -1) {
          // line contains the item
          index = line.indexOf("=");
          result = line.substring(index + 1);
          break;
        }
      }
    } else {
      break;
    }
  } while (line.length() > 0);

  return result;
}



//--------------------------------------------------------------------
String GetNextLine() {
  String line = "";

  for (aSearchIndex; aSearchIndex < aSDdata.length(); aSearchIndex++) {
    char nextChar = aSDdata.charAt(aSearchIndex);
    if (nextChar == '\r') {
      // ignore
    } else if (nextChar == '\n') {
      // we have the next line
      aSearchIndex++;
      break;
    } else {
      line += String(nextChar);
    }
  }

  return line;
}


//--------------------------------------------------------------------
// Create config file: Config.cnf
void CreateConfigFile() {
  boolean SDstatus = true;
  String outputFileName = CONFIG_FILE_NAME;

  Println("CreateConfigFile called");

  // Initialize SD card
  if (SD.begin(VSPI_CSO) == false) {
    Println("Card Mount Failed");
    SDstatus = false;
  }

  if (SDstatus == true) {
    int cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Println("No SD card attached");
      SDstatus = false;
    }
  }

  if (SDstatus == true) {
    Println("Initializing SD card...");

    if (SD.begin(VSPI_CSO) == false) {
      Println("ERROR - SD card initialization failed!");
      SDstatus = false;
    }
  }


  if (SDstatus == true) {

    // If the file doesn't exist, create the file
    // Create a file on the SD card and write the data labels
    File file = SD.open(outputFileName);  // This is the log file
    if (file == false) {
      Println("File doens't exist");
      Println("Creating file...");
      writeFile(SD, outputFileName.c_str(), "");
    } else {
      Println("File already exists");
    }
    file.close();

    String dataMessage = "";
    dataMessage = "# GPS Tracker Config File.  !! No empty lines !!\r\n";
    //dataMessage += "# The Unit. Default value is 1234567 \r\n";
    //dataMessage += "Unit=";
    //dataMessage += DEFAULT_UNIT;
    //dataMessage += "\r\n";
    dataMessage += "NetworkAPN=";
    dataMessage += NETWORK_APN;
    dataMessage += "\r\n";
    dataMessage += "tlf=";
    dataMessage += SMS_TARGET;
    dataMessage += "\r\n";
    dataMessage += "radius=";
    dataMessage += 50;
    dataMessage += "\r\n";
    dataMessage += "loginterval=";
    dataMessage += GPS_LOG_INTERVAL / 1000;
    dataMessage += "\r\n";
    dataMessage += "deepsleep=0";
    dataMessage += "\r\n";
    dataMessage += "logenable=true";
    dataMessage += "\r\n";
    dataMessage += "debug=2";
    dataMessage += "\r\n";

    Println("Save data: ");
    Println(dataMessage);

    appendFile(SD, outputFileName.c_str(), dataMessage.c_str());

#ifdef DEBUG
    // This is just for test
    //readFile(SD, outputFileName.c_str());
    printDirectory();
#endif
  }

  if (SDstatus == true) {
    aSD_CardStatus = "OK    ";
  } else {
    aSD_CardStatus = "ERROR6";
  }

  SD.end();  // Unmount
}


//--------------------------------------------------------------------
// write config file: Config.cnf
void WriteConfigFile(String dataMessage) {
  boolean SDstatus = true;
  String outputFileName = CONFIG_FILE_NAME;

  Println("writeConfigFile called");

  // Initialize SD card
  if (SD.begin(VSPI_CSO) == false) {
    Println("Card Mount Failed");
    SDstatus = false;
  }

  if (SDstatus == true) {
    int cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Println("No SD card attached");
      SDstatus = false;
    }
  }

  if (SDstatus == true) {
    Println("Initializing SD card...");

    if (SD.begin(VSPI_CSO) == false) {
      Println("ERROR - SD card initialization failed!");
      SDstatus = false;
    }
  }

  if (SDstatus == true) {
    writeFile(SD, outputFileName.c_str(), dataMessage.c_str());

    Println("Save data: ");
    Println(dataMessage);
  }

  if (SDstatus == true) {
    aSD_CardStatus = "OK    ";
  } else {
    aSD_CardStatus = "ERROR6";
  }

  SD.end();  // Unmount
}



//--------------------------------------------------------------------
// Create log file: YYYYMMDD.txt one file for each day
void SdLogger(String errorCondition) {
  boolean SDstatus;
  String outputFileName;
  String aYear;
  String aMonth;
  String aDay;
  struct tm timeinfo;

  Println("SdLogger called");

  if (getLocalTime(&timeinfo)) {
    aYear = String(timeinfo.tm_year + 1900);
    aMonth = String(timeinfo.tm_mon + 1);
    if (aMonth.length() < 2) {
      aMonth = "0" + aMonth;
    }

    aDay = String(timeinfo.tm_mday);
    if (aDay.length() < 2) {
      aDay = "0" + aDay;
    }

    Print("Date Found: ");
    Print(aYear);
    Print("-");
    Print(aMonth);
    Print("-");
    Println(aDay);
  }


  outputFileName = "/";
  outputFileName += aYear;   // get the year
  outputFileName += aMonth;  // get the month
  outputFileName += aDay;    // get the day
  outputFileName += ".log";

  // Mount SD card
  SDstatus = SD.begin(VSPI_CSO);
  if (SDstatus == false) {
    Println("Card Mount Failed");
  }

  if (SDstatus == true) {
    int cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Println("No SD card attached");
      SDstatus = false;
    }
  }


  if (SDstatus == true) {
    // If the file doesn't exist, create the file
    // Create a file on the SD card and write the data labels
    File file = SD.open(outputFileName);  // This is the log file
    if (file == false) {
      Println("File doens't exist");
      Println("Creating file...");
      writeFile(SD, outputFileName.c_str(), "");
    } else {
      Println("File already exists");
    }
    file.close();

    String dataMessage = aTimeStamp + "   " + errorCondition + "\r\n";

    Print("Save data: ");
    Println(dataMessage);

    SDstatus = appendFile(SD, outputFileName.c_str(), dataMessage.c_str());


#ifdef DEBUG
    // This is just for test
    //readFile(SD, outputFileName.c_str());
    //printDirectory();
#endif
  }

  if (SDstatus == true) {
    aSD_CardStatus = "OK ";
  } else {
    aSD_CardStatus = "ER7";
  }

  SD.end();  // Unmount
}



//--------------------------------------------------------------------
// Create log file: YYYYMMDD.KML
void SdGpsLogger(float posLat, float posLng, boolean closeFile) {
  boolean initFile = false;  // Set true is new file is created
  boolean SDstatus;
  String aYear;
  String aMonth;
  String aDay;
  String aHour;
  String aMin;
  String aSec;
  String dateTime;
  struct tm timeinfo;
  String line1;
  String line2;
  String line3;
  String line4;
  String line5;
  String line6;
  String line7;
  String dataMessage;

  Println("SdGpsLogger called");

  if (getLocalTime(&timeinfo)) {
    aYear = String(timeinfo.tm_year + 1900);

    aMonth = String(timeinfo.tm_mon + 1);
    if (aMonth.length() < 2) {
      aMonth = "0" + aMonth;
    }

    aDay = String(timeinfo.tm_mday);
    if (aDay.length() < 2) {
      aDay = "0" + aDay;
    }

    aHour = String(timeinfo.tm_hour);
    if (aHour.length() < 2) {
      aHour = "0" + aHour;
    }

    aMin = String(timeinfo.tm_min);
    if (aMin.length() < 2) {
      aMin = "0" + aMin;
    }

    dateTime = aYear + "-" + aMonth + "-" + aDay + " " + aHour + ":" + aMin;

    Print("Date Found: ");
    Println(dateTime);
  }

  if (aActiveGpsLogFile.length() == 0) {
    // Generate new file name
    aActiveGpsLogFile = "/";
    aActiveGpsLogFile += aYear;   // get the year
    aActiveGpsLogFile += aMonth;  // get the month
    aActiveGpsLogFile += aDay;    // get the day
    aActiveGpsLogFile += aHour;   // get the day
    aActiveGpsLogFile += aMin;    // get the day
    aActiveGpsLogFile += ".KML";
  }

  // Mount SD card
  SDstatus = SD.begin(VSPI_CSO);
  if (SDstatus == false) {
    Println("Card Mount Failed");
  }

  if (SDstatus == true) {
    int cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Println("No SD card attached");
      SDstatus = false;
    }
  }


  if (SDstatus == true) {
    // If the file doesn't exist, create the file
    // Create a file on the SD card and write the data labels
    File file = SD.open(aActiveGpsLogFile);  // This is the log file
    if (file == false) {
      Println("File doens't exist");
      Println("Creating file...");

      initFile = true;
      writeFile(SD, aActiveGpsLogFile.c_str(), "");

      // Write header info
      // <?xml version="1.0" encoding="UTF-8"?>
      // <kml xmlns="http://www.opengis.net/kml/2.2">
      // <Document>

      line1 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
      line2 = "<kml xmlns=\"http://earth.google.com/kml/2.0\">\r\n";
      line3 = "<Document>\r\n";
      line4 = "<open>1</open>\r\n";
      line5 = "<name>Data from GPS logger</name>\r\n";
      line6 = "<Folder>";
      line7 = "<name>" + dateTime + "</name>";
      dataMessage = line1 + line2 + line3 + line4 + line5 + line6 + line7;

      Print("Write header data: ");
      Println(dataMessage);

      SDstatus = appendFile(SD, aActiveGpsLogFile.c_str(), dataMessage.c_str());
    } else {
      Println("File already exists");
    }

    file.close();

    // Write the normal GPS waypoint entry
    //String gpsSpeed = String(gps.speed.kmph(), 1);
    String gpsSpeed = String(aGpsSpeed, 0);
    String gpsLng = String(posLng, 6);
    String gpsLat = String(posLat, 6);

    line1 = "<Placemark>\r\n";
    line2 = "<name>" + aHour + ":" + aMin + ", " + gpsSpeed + "</name>\r\n";
    line3 = "<Point><coordinates>" + gpsLng + "," + gpsLat + "</coordinates></Point></Placemark>\r\n";
    dataMessage = line1 + line2 + line3;

    Print("Write log data: ");
    Println(dataMessage);

    SDstatus = appendFile(SD, aActiveGpsLogFile.c_str(), dataMessage.c_str());


    if (closeFile == true) {
      // Write footer to file
      line1 = "</Folder>\r\n";
      line2 = "</Document>\r\n";
      line3 = "</kml>\r\n";
      dataMessage = line1 + line2 + line3;

      Print("Write footer data: ");
      Println(dataMessage);

      SDstatus = appendFile(SD, aActiveGpsLogFile.c_str(), dataMessage.c_str());

      aActiveGpsLogFile = "";
    }


#ifdef DEBUG
    // This is just for test
    //readFile(SD, aActiveGpsLogFile.c_str());
    //printDirectory();
#endif
  }

  if (SDstatus == true) {
    aSD_CardStatus = "OK ";
  } else {
    aSD_CardStatus = "ER7";
  }

  SD.end();  // Unmount
}




//---------------------------------------------------------------------------------------------------
// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS& fs, const char* path, const char* message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);

  if (file == false) {
    Println("Failed to open file for writing");
    return;
  }

  if (message[0] != 0) {  // To avoid write failed at empty message
    if (file.print(message) == true) {
      Serial.println("File written");
    } else {
      Serial.println("Write failed");
    }
  }

  file.close();
}



//---------------------------------------------------------------------------------------------------
// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
boolean appendFile(fs::FS& fs, const char* path, const char* message) {
  boolean result = true;

#ifdef DEBUG
  Serial.printf("Appending to file: %s\n", path);
#endif

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return false;
  }
  if (file.print(message)) {
    Println("Text appended");
  } else {
    Println("Append failed");
    result = false;
  }

  file.close();

  return result;
}

//---------------------------------------------------------------------------------------------------
void readFile(fs::FS& fs, const char* path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Println("Failed to open file for reading");
    return;
  }

  Println("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }

  file.close();
}


//---------------------------------------------------------------------------------------------------
void printDirectory() {
  boolean loop = true;
  File dir = SD.open("/");

  while (loop) {
    File entry = dir.openNextFile();
    if (entry == false) {
      // no more files
      entry.close();
      loop = false;
      //break;
    } else if (entry.isDirectory() == false) {
      String fileName = entry.name();
      String characterToTest = "/._";  // This is to remove a strange extra entry
      if (fileName.startsWith(characterToTest) == false) {
        Serial.println(entry.name());
      }
    }

    entry.close();
  }

  return;
}



//********************************************************************************
// GPS Core 0
//********************************************************************************
//////////////////////////////////////////////////////////////////////////////////
// Core 0, GPS task
//----------------------------------------------------------------------
// Read GPS
void MainTask0(void* parameter) {
  Serial.print("Program start (GPS). Core:");
  Serial.println(xPortGetCoreID());


  //---------------------------------------------------------------------------
  // Loop
  //---------------------------------------------------------------------------
  while (1 == 1) {
    static int lastSec;

    while (SerialGPS.available()) {
      int c = SerialGPS.read();
      if (gps.encode(c)) {
        // if (gps.time.isValid() == true) {
        if (gps.date.isValid() && gps.time.isValid()) {
          if (gps.time.second() != lastSec) {  // Do only call after sec change
            updateTime();
            readGps();
            displayInfo();
            lastSec = gps.time.second();
          }
        }
      }
    }

    if (millis() > 30000 && gps.charsProcessed() < 10) {
      Serial.println(F("No GPS detected: check wiring."));
      delay(1000);
    }

    delay(100);
  }  // while..

  vTaskDelete(NULL);
}


//----------------------------------------------------------------------------------
void gpsSleep() {
  digitalWrite(BOARD_GPS_WAKEUP_PIN, LOW);  // or HIGH depending on your module logic
}


//----------------------------------------------------------------------------------
void gpsWake() {
  digitalWrite(BOARD_GPS_WAKEUP_PIN, HIGH);  // opposite of sleep state
}



//----------------------------------------------------------------------------------
// Called each second change
void updateTime() {
  struct tm timeinfo;

  // Set the aTimeStamp string
  if (gps.date.isValid() && gps.time.isValid()) {

    // The the local CPU RTC time
    timeinfo.tm_year = gps.date.year() - 1900;  // Year -1900
    timeinfo.tm_mon = gps.date.month() - 1;     // Month -1
    timeinfo.tm_mday = gps.date.day();
    timeinfo.tm_hour = gps.time.hour();
    timeinfo.tm_min = gps.time.minute();
    timeinfo.tm_sec = gps.time.second();
    time_t t = mktime(&timeinfo);  // unsigned long

    // Timezone Denmark
    t += 3600;
    // Daylight Saving Time
    if (isDst(timeinfo) == true) {
      t += 3600;
    }


    //    printf("Setting time: %s", asctime(&timeinfo));

    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);


    getLocalTime(&timeinfo);

    int year = timeinfo.tm_year + 1900;
    if (year >= 2025) {
      aTimeStamp = String(timeinfo.tm_year + 1900);
      aTimeStamp += "-";
      if ((timeinfo.tm_mon + 1) < 10) {
        aTimeStamp += "0";
      }
      aTimeStamp += String(timeinfo.tm_mon + 1);
      aTimeStamp += "-";
      if (timeinfo.tm_mday < 10) {
        aTimeStamp += "0";
      }
      aTimeStamp += String(timeinfo.tm_mday);
      aTimeStamp += " ";
      if (timeinfo.tm_hour < 10) {
        aTimeStamp += "0";
      }
      aTimeStamp += String(timeinfo.tm_hour);
      aTimeStamp += ":";
      if (timeinfo.tm_min < 10) {
        aTimeStamp += "0";
      }
      aTimeStamp += String(timeinfo.tm_min);
      aTimeStamp += ":";
      if (timeinfo.tm_sec < 10) {
        aTimeStamp += "0";
      }
      aTimeStamp += String(timeinfo.tm_sec);
    }

    // Serial.println(aTimeStamp);
  }
}



//----------------------------------------------------------------------
// Check if local ESP date is within Daylight Saving Time
boolean isDst(tm timeinfo) {
  int yearx;
  int monthx;
  int dayx;
  int hourx;
  int dowx;
  //  struct tm timeinfo;
  boolean dst = false;

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  } else {
    yearx = timeinfo.tm_year + 1900;
    monthx = timeinfo.tm_mon + 1;
    dayx = timeinfo.tm_mday;
    hourx = timeinfo.tm_hour;
    dowx = timeinfo.tm_wday;  // Get the day of the week. 0 = Sunday, 6 = Saturday

    int previousSunday = dayx - dowx;
    int nextSunday = previousSunday + 7;

    if ((monthx > 3) && (monthx < 10)) {
      dst = true;  // DST is happening in Europe!
    }

    // In March, we are DST if our previous Sunday was on or after the 25th.
    if (monthx == 3) {
      if ((dayx >= 25) && (((dowx == 0) && (hourx > 1)) || (dayx > nextSunday))) {
        dst = true;
      }
    }

    // In October we must be before the last Sunday to be dst.
    // In this case we are changing time at 2:00 so the change to the previous Sunday
    // happens at midnight so the previous Sunday is actually this Sunday at 2:00 AM
    // That means the previous Sunday must be on or before the 31st.
    if (monthx == 10) {
      if ((dayx < 25) || ((nextSunday <= 31) && (dayx < nextSunday)) || ((dowx == 0) && (hourx <= 1))) {
        dst = true;  // Changes at 2:00
      }
    }
  }

  return dst;
}





//----------------------------------------------------------------------------------
//gps.location.isValid()
//gps.location.lat()
//gps.location.lng()
//gps.date.isValid()
//gps.date.year()
//gps.date.month()
//gps.date.day()
//gps.time.isValid()
//gps.time.hour()
//gps.time.minute()
//gps.time.second()
//gps.time.centisecond()
//gps.satellites();
//gps.hdop()
//gps.speed();
//gps.altitude();
//gps.course();
void readGps() {
  if (gps.location.isValid()) {
    aGpsIsValid = true;
    aGpsLat = gps.location.lat();
    aGpsLng = gps.location.lng();
  } else {
    aGpsIsValid = false;
  }

  if (gps.satellites.isValid()) {
    aGpsSattelites = gps.satellites.value();
  } else {
    aGpsSattelites = 0;
  }

  if (gps.speed.isValid()) {
    aGpsSpeed = gps.speed.kmph();
  } else {
    aGpsSpeed = 0;
  }

  if (gps.course.isValid()) {
    aGpsCourse = gps.course.deg();
  } else {
    aGpsCourse = 0;
  }

  if (gps.altitude.isValid()) {
    aGpsAltitude = gps.altitude.meters();
  } else {
    aGpsAltitude = 0;
  }
}




//----------------------------------------------------------------------------------
//gps.location.isValid()
//gps.location.lat()
//gps.location.lng()
//gps.date.isValid()
//gps.date.year()
//gps.date.month()
//gps.date.day()
//gps.time.isValid()
//gps.time.hour()
//gps.time.minute()
//gps.time.second()
//gps.time.centisecond()
//gps.satellites();
//gps.hdop()
//gps.speed();
//gps.altitude();
//gps.course();
void displayInfo() {

#ifdef DEBUG2
  Serial.print(F("Location: "));
  if (gps.location.isValid()) {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(", "));
    Serial.print(gps.location.lng(), 6);
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid()) {
    Serial.print(gps.date.year());
    Serial.print(F("-"));
    if (gps.date.month() < 10) {
      Serial.print(F("0"));
    }
    Serial.print(gps.date.month());
    Serial.print(F("-"));
    if (gps.date.day() < 10) {
      Serial.print(F("0"));
    }
    Serial.print(gps.date.day());
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid()) {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
  } else {
    Serial.print(F("INVALID"));
  }

  //Serial.println();

  Serial.print(F("Satellites: "));
  if (gps.satellites.isValid()) {
    int sats = gps.satellites.value();
    Serial.print(sats, 0);
  } else {
    Serial.print(F("INVALID"));
  }

  // Horizontal Dilution of Precision
  // HDOP Values and Their Implications
  // 1 to 2: Excellent accuracy, suitable for critical operations.
  // 2 to 5: Good accuracy, sufficient for most offshore tasks.
  // Above 5: Poor accuracy, requiring alternative or redundant reference systems.
  Serial.print(F("  Hdop: "));
  if (gps.hdop.isValid()) {
    Serial.print(gps.hdop.value(), 0);
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Speed: "));
  if (gps.speed.isValid()) {
    Serial.print(gps.speed.kmph(), 1);
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Course: "));
  if (gps.course.isValid()) {
    Serial.print(gps.course.deg(), 1);
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Altitude: "));
  if (gps.altitude.isValid()) {
    Serial.print(gps.altitude.meters(), 0);
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.println();
#endif
}


//---------------------------------------------------------------
// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

//---------------------------------------------------------------
// STRINGS_SIZE 30    aStrings[STRINGS_SIZE];
// string: string to parse
// returns number of items parsed
int split(char splitChar, String msgString, String strings[]) {
  int bufferIndex = 0;
  strings[bufferIndex] = "";

  for (int index = 0; index < msgString.length(); ++index) {
    if (msgString[index] != splitChar) {
      strings[bufferIndex] += msgString[index];
    } else {
      bufferIndex++;
      if (bufferIndex >= STRINGS_SIZE) {
        break;
      } else {
        strings[bufferIndex] = "";
      }
    }
  }

  bufferIndex++;

  return bufferIndex;
}

//----------------------------------------------------------------------------------
void setLocalDefaultTime() {
  struct tm tm;

  tm.tm_year = 2025 - 1900;
  tm.tm_mon = 9 - 1;
  tm.tm_mday = 20;
  tm.tm_hour = 10;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  time_t t = mktime(&tm);
#ifdef DEBUG
  printf("Setting time: %s", asctime(&tm));
#endif
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}
