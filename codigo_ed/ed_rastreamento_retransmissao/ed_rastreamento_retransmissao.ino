#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <list>

#include <iostream>
#include <stdint.h>
#include <string>
#include <time.h>

using namespace std;

#include "gps.h"
#include "axp20x.h"
#include "flash_memory.h"

gps gps;
flash_memory flash_memory;

AXP20X_Class axp;
TinyGPSPlus tGps;
HardwareSerial GPS(1);

#ifdef COMPILE_REGRESSION_TEST
# define FILLMEIN 0
#else
# warning "You must replace the values marked FILLMEIN with real values from the TTN control panel!"
# define FILLMEIN (#dont edit this, edit the lines that use FILLMEIN)
#endif

//LSB
static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui (u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}

// LSB
static const u1_t PROGMEM DEVEUI[8] = { 0xAE, 0x75, 0x04, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getDevEui (u1_t* buf) {
  memcpy_P(buf, DEVEUI, 8);
}

// MSB.
static const u1_t PROGMEM APPKEY[16] = { 0x4A, 0xD3, 0x37, 0xF7, 0xB7, 0x2F, 0xFD, 0x98, 0x78, 0xC2, 0xC3, 0x62, 0x78, 0x47, 0xCB, 0xA0 };
void os_getDevKey (u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}

// Variáveis
//uint8_t mydata[] = "Mensagem";
uint8_t txBuffer_send[10]; // used to store message sent

uint8_t txBuffer[] = "";
std::list<int> list_fcnt_retransmission;
int count_block = 0;
int count_retransmission = 0;
int count_retransmission_block = 0;

// CREATE MENSAGEM
struct mensagem {
  int fcnt;
  double latitude;
  double longitude;
};
std::list<mensagem> list_mensagens;

static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 15;

// Pin mapping for TTGO TBeam V1.1
const lmic_pinmap lmic_pins = {
  .nss = 18,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 14,
  .dio = {26, 33, 32},
};

void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
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
      {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        Serial.print("netid: ");
        Serial.println(netid, DEC);
        Serial.print("devaddr: ");
        Serial.println(devaddr, HEX);
        Serial.print("artKey: ");
        for (int i = 0; i < sizeof(artKey); ++i) {
          Serial.print(artKey[i], HEX);
        }
        Serial.println("");
        Serial.print("nwkKey: ");
        for (int i = 0; i < sizeof(nwkKey); ++i) {
          Serial.print(nwkKey[i], HEX);
        }
        Serial.println("");
      }
      Serial.println(F("Successful OTAA Join..."));
      // Disable link check validation (automatically enabled
      // during join, but because slow data rates change max TX
      // size, we don't use it in this example.
      LMIC_setLinkCheckMode(0);
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received ack"));
      if (LMIC.dataLen) {
        Serial.print(F("Received packet: "));
        Serial.println(LMIC.dataLen);

        //Recebe mensagem de retransmissão
        int fcnt_reenviar;
        for (int i = 0; i < LMIC.dataLen; i = i + 2)
        {
          fcnt_reenviar = ( LMIC.frame[LMIC.dataBeg + i] << 8 ) + LMIC.frame[LMIC.dataBeg + i + 1];
          Serial.println((String) "fcnt_reenviar: " + fcnt_reenviar);
          //send_retransmisao(&sendjob, fcnt_reenviar);
          list_fcnt_retransmission.push_back(fcnt_reenviar);
        }
      }
      if (list_fcnt_retransmission.empty()) {
        Serial.println("-------ELSE-------");
        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      } else {
        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(5), do_send);
      }

      // Schedule next transmission
      //os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
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
    case EV_TXSTART:
      Serial.println(F("EV_TXSTART"));
      Serial.println(LMIC.freq);
      break;
    default:
      Serial.print(F("Unknown event: "));
      Serial.println((unsigned) ev);
      break;
  }
}
//Função de agendamento, pode criar a função para agendar
void do_send(osjob_t* j) {
  if (LMIC.opmode & OP_TXRXPEND)
  {
    Serial.println(F("OP_TXRXPEND, not sending"));
  }
  else
  {
    Serial.println(F("-- SEND-- "));
    float latitude = 0;
    float longitude = 0;
    int fcnt_reenviar = 0;
    
    // SEND PACKEGE
    if (!list_fcnt_retransmission.empty()) {
      Serial.println("---- RETRANSMISSAO ----");
      fcnt_reenviar = list_fcnt_retransmission.front();
      // GET MENSAGEM
      for (std::list<mensagem>::const_iterator it = list_mensagens.begin(), end = list_mensagens.end(); it != end; ++it) {
        if (fcnt_reenviar == (*it).fcnt) {
          latitude = (*it).latitude;
          longitude = (*it).longitude;
        }
        //Serial.print((String) "fcnt: " + (*it).fcnt + " " + (*it).latitude + " " + (*it).longitude + " | ") ;
      }
      list_fcnt_retransmission.pop_front();

      gps.buildPacket(txBuffer_send, latitude, longitude);
      txBuffer_send[9] = fcnt_reenviar & 0xFF;
      Serial.println((String) "Seq Num: " + LMIC.seqnoUp + "/" + fcnt_reenviar  + " " + latitude + " " + longitude);
      LMIC_setTxData2(1, txBuffer_send, sizeof(txBuffer_send), 0);
    } else if (gps.checkGpsFix()) {
      Serial.println("---- NOVA MENSAGEM ----");
      // MENSAGEM GPS
      gps.get_coordenates(&latitude, &longitude);

      // ADD MENSAGEM
      struct mensagem msg_atual = {LMIC.seqnoUp, latitude, longitude};
      list_mensagens.push_back(msg_atual);
      Serial.print("----LISTA:  ");
      for (std::list<mensagem>::const_iterator it = list_mensagens.begin(), end = list_mensagens.end(); it != end; ++it) {
        Serial.print((String) "fcnt: " + (*it).fcnt + " " + (*it).latitude + " " + (*it).longitude + " | ") ;
      }
      Serial.println("  :LISTA----");
      
      gps.buildPacket(txBuffer_send, latitude, longitude);
      txBuffer_send[9] = fcnt_reenviar & 0xFF;
      Serial.println((String) "Seq Num: " + LMIC.seqnoUp + "/" + fcnt_reenviar  + " " + latitude + " " + longitude);
      LMIC_setTxData2(1, txBuffer_send, sizeof(txBuffer_send), 0);
    }else{
      Serial.println("---- MENSAGEM NÃO ENVIADA ----");
      //try again in 3 seconds
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(3), do_send);
    }
  }
}

void setup() {
  Serial.println(LMIC.freq);
  Serial.begin(115200);
  Serial.println(F("Starting"));

  // Power GPS
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
  GPS.begin(9600, SERIAL_8N1, 34, 12);

  gps.init();

  // Memory init
  //flash_memory.init();
  //flash_memory.reset_memory();
  //flash_memory.print_memory();

#ifdef VCC_ENABLE
  // For Pinoccio Scout boards
  pinMode(VCC_ENABLE, OUTPUT);
  digitalWrite(VCC_ENABLE, HIGH);
  delay(1000);
#endif

#if defined(CFG_us921)
  Serial.println(F("Loading AU915/AU921 Configuration..."));
#endif

  // LMIC init, inicia a contagem de tempo do lmic
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  LMIC_setDrTxpow(DR_SF7, 14);//tipo de transmissão que vai enviar
  LMIC_selectSubBand(1);
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);//erro de tempo entro o ed e gw, se tiver muito diferente faz 10/100 50/100...

  // Start job (sending automatically starts OTAA too)
  do_send(&sendjob);
}

void loop() {
  os_runloop_once();//loop do lmic
}
