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
