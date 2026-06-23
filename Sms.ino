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

    //-------------------------------------------------------------------------
    if ((line1.indexOf("seton") > -1) || (line1.indexOf("son") > -1)) {
      SetOn(line1, sender);
      cmdFound = true;
    }

    //-------------------------------------------------------------------------
    if ((line1.indexOf("setoff") > -1) || (line1.indexOf("soff") > -1)) {
      SetOff(line1, sender);
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
      String lin9 = "SetOn\n";
      String lin10 = "SetOff\n";
      String SMSText = lin3 + lin4 + lin5 + lin6 + lin7 + lin8 + lin9 + lin10;
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
void SetOn(String line1, String sender) {
  String SMSText = "";

  Serial.println("seton received");

  digitalWrite(ON_OFF_PIN, HIGH);

  String lin2 = "Cmd: " + line1 + "\n";
  String lin3 = "ON_OFF_PIN set ON\n";
  SMSText = lin2 + lin3;

  SendSMS(SMSText, sender);
}


//--------------------------------------------------------------------
void SetOff(String line1, String sender) {
  String SMSText = "";

  Serial.println("setoff received");

  digitalWrite(ON_OFF_PIN, LOW);

  String lin2 = "Cmd: " + line1 + "\n";
  String lin3 = "ON_OFF_PIN set OFF\n";
  SMSText = lin2 + lin3;

  SendSMS(SMSText, sender);
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
