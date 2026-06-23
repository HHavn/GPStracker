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


//----------------------------------------------------------------------------------
void gpsSleep() {
  digitalWrite(BOARD_GPS_WAKEUP_PIN, LOW);  // or HIGH depending on your module logic
}


//----------------------------------------------------------------------------------
void gpsWake() {
  digitalWrite(BOARD_GPS_WAKEUP_PIN, HIGH);  // opposite of sleep state
}
