//********************************************************************************
// GPS Core 0
//********************************************************************************
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
