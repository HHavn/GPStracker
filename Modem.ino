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
