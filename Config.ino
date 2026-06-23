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
