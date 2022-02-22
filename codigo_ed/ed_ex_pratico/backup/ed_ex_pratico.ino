#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <list>

#include <iostream>
#include <stdint.h>
#include <string>
#include <time.h>

using namespace std;


#ifdef COMPILE_REGRESSION_TEST
# define FILLMEIN 0
#else
# warning "You must replace the values marked FILLMEIN with real values from the TTN control panel!"
# define FILLMEIN (#dont edit this, edit the lines that use FILLMEIN)
#endif

//LSB
static const u1_t PROGMEM APPEUI[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// LSB
static const u1_t PROGMEM DEVEUI[8]= { 0xAE, 0x75, 0x04, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// MSB.
static const u1_t PROGMEM APPKEY[16] = { 0x4A, 0xD3, 0x37, 0xF7, 0xB7, 0x2F, 0xFD, 0x98, 0x78, 0xC2, 0xC3, 0x62, 0x78, 0x47, 0xCB, 0xA0 };
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

// Variáveis
//uint8_t mydata[] = "Mensagem";
uint8_t txBuffer[] = "";
std::list<int> list_fcnt_retransmission;

struct mensagem {
    int fcnt;
    int dado;
};
std::list<mensagem> registro_mensagem;

static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 5;

// Pin mapping for TTGO TBeam V1.1
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26, 33, 32},
};

void send_retransmisao(osjob_t* j, int fcnt_reenviar){
  
    int dado;
    int fcnt;
    for (const mensagem & val : registro_mensagem){
        if(val.fcnt == fcnt_reenviar){
          dado = val.dado;
          fcnt = val.fcnt;
          break;
        }
    }
//    uint8_t enviar_dado[3] = { highByte(dado), lowByte(dado)};
    Serial.println();
    Serial.print("DADO REENVIADO- fcnt_reenviar:");
    Serial.print(fcnt_reenviar);
    Serial.print(" dado: ");
    Serial.print(dado);
    Serial.print(" val.fcnt: ");
    Serial.print(fcnt);
    Serial.print(" val.dado: ");
    Serial.println(dado);

    uint8_t enviar_dado[2] = {highByte(dado), lowByte(dado)};
    uint8_t enviar_fcnt[2] = {highByte(fcnt), lowByte(fcnt)};

    int size_of_dados = sizeof(enviar_dado);
    int size_of_fcnt = sizeof(enviar_fcnt);
    uint8_t enviar_mensage[size_of_dados+ size_of_fcnt+1];

    memcpy(enviar_mensage, enviar_dado, size_of_dados);
    memcpy(enviar_mensage + size_of_dados, enviar_fcnt, size_of_fcnt);

   if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        LMIC_setTxData2(1, enviar_mensage, sizeof(enviar_mensage)-1, 0);
        Serial.println(F("Retransmission packet queued"));
        Serial.print(F("Sending packet on frequency: "));
        Serial.println(LMIC.freq);
    }
}


void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
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
              for (int i=0; i<sizeof(artKey); ++i) {
                Serial.print(artKey[i], HEX);
              }
              Serial.println("");
              Serial.print("nwkKey: ");
              for (int i=0; i<sizeof(nwkKey); ++i) {
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
              
              int fcnt_reenviar;
              for (int i=0; i < LMIC.dataLen; i++)
              {
                Serial.print(LMIC.frame[LMIC.dataBeg + i], HEX);
                fcnt_reenviar = LMIC.frame[LMIC.dataBeg + i];
                txBuffer[i] = fcnt_reenviar;
                
                list_fcnt_retransmission.push_back(fcnt_reenviar);
                Serial.print(" ");
              }
              send_retransmisao(&sendjob, fcnt_reenviar);

              for (int val : list_fcnt_retransmission){
                Serial.print(val);
                Serial.print(" ");
                }
              Serial.println();
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
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
void do_send(osjob_t* j){
    
    int fcnt = LMIC.seqnoUp;
    int dado = rand() % 100 + 1;
    uint8_t enviar_dado[2] = {highByte(dado), lowByte(dado)};
    uint8_t enviar_fcnt[2] = {highByte(fcnt), lowByte(fcnt)};

    Serial.print(F("Mensagem: "));
    Serial.println(dado);
    Serial.print(F("SeqNo: "));
    Serial.println(LMIC.seqnoUp);// https://www.thethingsnetwork.org/forum/t/where-in-lmic-is-the-fcnt-parameter-stored/3082/2

    // Registra um novo dado na memória de dados
    registro_mensagem.push_back({LMIC.seqnoUp, dado});
    // Acessa dados da memória
    Serial.println("Memória: ");
    for (const mensagem & val : registro_mensagem){
        Serial.print((String) val.fcnt + "-" + val.dado + " | ");
    }
    Serial.println();

    int size_of_dados = sizeof(enviar_dado);
    int size_of_fcnt = sizeof(enviar_fcnt);
    uint8_t enviar_mensage[size_of_dados+ size_of_fcnt+1];

    memcpy(enviar_mensage, enviar_dado, size_of_dados);
    memcpy(enviar_mensage + size_of_dados, enviar_fcnt, size_of_fcnt);

    
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
        Serial.print(F("SeqNo: "));
        Serial.println(LMIC.seqnoUp); 
//        LMIC.seqnoUp = 1;
        LMIC_setTxData2(1, enviar_mensage, sizeof(enviar_mensage)-1, 0); //canal, dado, tamanho do dado, flag se quer confirmação ou não 0 = sem confirmação
        Serial.println(F("Packet queued"));
        Serial.print(F("Sending packet on frequency: "));
        Serial.println(LMIC.freq);
    }
    if ((fcnt+1)%7 == 0) {
      int fcnt = LMIC.seqnoUp;
      LMIC.seqnoUp  = LMIC.seqnoUp+1;
      int dado = rand() % 100 + 1;
      registro_mensagem.push_back({fcnt, dado});
      Serial.println(F("Packet NOT queued"));
      Serial.print(F("Mensagem: "));
      Serial.println(dado);
      Serial.print((String)"fcnt: " + fcnt + " | SeqNo: "+ LMIC.seqnoUp);
      }
    // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
    Serial.println(LMIC.freq);
    Serial.begin(115200);
    Serial.println(F("Starting"));

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