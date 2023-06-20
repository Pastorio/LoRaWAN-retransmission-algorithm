#pragma once

#include "FS.h"
#include "SPIFFS.h"

#define FORMAT_SPIFFS_IF_FAILED true

class memory
{
    public:
        void init();
        void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
        void readFile(fs::FS &fs, const char * path);
        
        void appendFile(fs::FS &fs, const char * path, const char * message);
        void appendFileDouble(fs::FS &fs, const char * path, double message);
        void writeFile(fs::FS &fs, const char * path, const char * message);
        
        void deleteFile(fs::FS &fs, const char * path);
    private:
        unsigned int count = 1; 
        bool logger=true;
        int totalSize=0;

};
