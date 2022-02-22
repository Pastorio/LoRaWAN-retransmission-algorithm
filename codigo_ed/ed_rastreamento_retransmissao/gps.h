#pragma once


#include <TinyGPS++.h>
#include <HardwareSerial.h>


#define GPS_TX 34
#define GPS_RX 12


class gps
{
    public:
        void init();
        bool checkGpsFix();
        void buildPacket(uint8_t txBuffer[9], double gps_lat, double gps_long);
        void encode();
        void get_coordenates(float *latitude, float *longitude);

    private:
        uint32_t LatitudeBinary, LongitudeBinary;
        uint16_t altitudeGps;
        uint8_t hdopGps;
        char t[32]; // used to sprintf for Serial output
        TinyGPSPlus tGps;
};
