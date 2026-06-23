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

#define ON_OFF_PIN 32

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

  pinMode(ON_OFF_PIN, OUTPUT);
  digitalWrite(ON_OFF_PIN, LOW);

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
