#include <Preferences.h>
#include <lmic.h>
#include <tuple>

class flash_memory
{
    public:
        void init();
        void reset_memory();
        void print_memory();
        void save_data(uint32_t seqNum, float lat_data, float long_data);
        void get_data(uint32_t seqNum, float *latitude, float *longitude); 
        
    private:
        Preferences preferences;
        unsigned int count = 1; 
        int totalSize = 0;
        unsigned int lenPrefMemory = 100;

        unsigned int memory_pos;
        unsigned int memory_count;
};
