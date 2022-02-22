#include <lmic.h>
#include <hal/hal.h>

#include "device_config.h"
#include "gps.h"

#include <WiFi.h>
#include <esp_sleep.h>

#include "axp20x.h"
AXP20X_Class axp;
TinyGPSPlus tGps;

// T-Beam specific hardware
#undef BUILTIN_LED
#define BUILTIN_LED 21

char s[32]; // used to sprintf for Serial output
uint8_t txBuffer[9];
gps gps;
static osjob_t sendjob;

// Those variables keep their values after software restart or wakeup from sleep, not after power loss or hard reset !
RTC_NOINIT_ATTR int RTCseqnoUp, RTCseqnoDn;
#ifdef USE_OTAA
RTC_NOINIT_ATTR u4_t otaaDevAddr;
RTC_NOINIT_ATTR u1_t otaaNetwKey[16];
RTC_NOINIT_ATTR u1_t otaaApRtKey[16];
#endif

// Schedule TX every this many seconds (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 20;

// Pin mapping for TBeams, might not suit the latest version > 1.0 ?
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26, 33, 32},
};

// These callbacks are only used in over-the-air activation.
#ifdef USE_OTAA
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16);}
#else
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
#endif

void storeFrameCounters()
{
  RTCseqnoUp = LMIC.seqnoUp;
  RTCseqnoDn = LMIC.seqnoDn;
  sprintf(s, "Counters stored as %d/%d", LMIC.seqnoUp, LMIC.seqnoDn);
  Serial.println(s);
}

void restoreFrameCounters()
{
  LMIC.seqnoUp = RTCseqnoUp;
  LMIC.seqnoDn = RTCseqnoDn;
  sprintf(s, "Restored counters as %d/%d", LMIC.seqnoUp, LMIC.seqnoDn);
  Serial.println(s);
}

void setOrRestorePersistentCounters()
{
  esp_reset_reason_t reason = esp_reset_reason();
  if ((reason != ESP_RST_DEEPSLEEP) && (reason != ESP_RST_SW))
  {
    Serial.println(F("Counters both set to 0"));
    LMIC.seqnoUp = 0;
    LMIC.seqnoDn = 0;
  }
  else
  {
    restoreFrameCounters();
  }
}

void onEvent (ev_t ev) {
  switch (ev) {
    case EV_SCAN_TIMEOUT:
      Serial.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      Serial.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      Serial.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      Serial.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      Serial.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      Serial.println(F("EV_JOINED"));
#ifdef USE_OTAA    
      otaaDevAddr = LMIC.devaddr;
      memcpy_P(otaaNetwKey, LMIC.nwkKey, 16);
      memcpy_P(otaaApRtKey, LMIC.artKey, 16);
      sprintf(s, "got devaddr = 0x%X", LMIC.devaddr);
      Serial.println(s);
#endif
      // Disable link check validation (automatically enabled
      // during join, but not supported by TTN at this time).
      LMIC_setLinkCheckMode(0);
      // TTN uses SF9 for its RX2 window.
      LMIC.dn2Dr = DR_SF9;
      break;
    case EV_RFU1:
      Serial.println(F("EV_RFU1"));
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      digitalWrite(BUILTIN_LED, LOW);
      if (LMIC.txrxFlags & TXRX_ACK) {
        Serial.println(F("Received Ack"));
      }
      if (LMIC.dataLen) {
        sprintf(s, "Received %i bytes of payload", LMIC.dataLen);
        Serial.println(s);
        sprintf(s, "RSSI %d SNR %.1d", LMIC.rssi, LMIC.snr);
        Serial.println(s);
      }
      storeFrameCounters();
      // Schedule next transmission
      Serial.println("Good night...");
      esp_sleep_enable_timer_wakeup(TX_INTERVAL*1000000);
      esp_deep_sleep_start();
      do_send(&sendjob);
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    default:
      Serial.println(F("Unknown event"));
      break;
  }
}

void do_send(osjob_t* j) {  

  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND)
  {
    Serial.println(F("OP_TXRXPEND, not sending"));
  }
  else
  { 
    if (gps.checkGpsFix())
    {
      // Prepare upstream data transmission at the next possible time.
      gps.buildPacket(txBuffer, tGps.location.lat(), tGps.location.lng());
      LMIC_setTxData2(1, txBuffer, sizeof(txBuffer), 0);
      Serial.println(F("Packet queued"));
      //Serial.println(LMIC.freq); //
      digitalWrite(BUILTIN_LED, HIGH);
    }
    else
    {
      //try again in 3 seconds
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(3), do_send);
    }
  }
  // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin(21, 22);
  if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
  Serial.println("AXP192 started");
  } else {
  Serial.println("AXP192 failed");
  }
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON); // Lora on T-Beam V1.0
  axp.setPowerOutPut(AXP192_LDO3, AXP202_ON); // Gps on T-Beam V1.0
  axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
  axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON); // OLED on T-Beam v1.0
  axp.setDCDC1Voltage(3300);
  axp.setChgLEDMode(AXP20X_LED_BLINK_1HZ);
  axp.adc1Enable(AXP202_BATT_CUR_ADC1, 1);
  
  //Turn off WiFi and Bluetooth
  WiFi.mode(WIFI_OFF);
  btStop();
  gps.init();

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  
#ifdef USE_OTAA
  esp_reset_reason_t reason = esp_reset_reason();
  if ((reason == ESP_RST_DEEPSLEEP) || (reason == ESP_RST_SW))
  {
    LMIC_setSession(0x1, otaaDevAddr, otaaNetwKey, otaaApRtKey);
  }
#else // ABP
  LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);

  // TTN uses SF9 for its RX2 window.
  LMIC.dn2Dr = DR_SF12;

  // Disable link check validation
  LMIC_setLinkCheckMode(0);
#endif

  // This must be done AFTER calling LMIC_setSession !
  setOrRestorePersistentCounters();

#ifdef CFG_au915
  LMIC_selectSubBand(1);

  //Disable FSB2-8, channels 16-72
  for (int b = 0; b < 8; ++b) {
      LMIC_disableSubBand(b);
    }
    LMIC_enableChannel(8);
#endif

  // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
  LMIC_setDrTxpow(DR_SF7,14);

  do_send(&sendjob);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  
}

void loop() {
    os_runloop_once();
}
