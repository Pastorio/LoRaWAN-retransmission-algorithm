#include "flash_memory.h"

void flash_memory::init()
{
  Serial.println(" -- Init flash memory --");
  
  preferences.begin("my-app", false); 

  memory_pos = preferences.getUInt("memory_pos", 0);
  memory_count = preferences.getUInt("memory_count", 0);
  Serial.printf("\n Última posição registrada: %d \n", memory_pos);
}

void flash_memory::reset_memory()
{
  Serial.println(" -- Reset memory --");
  
  memory_pos = 0;
  memory_count = 0;
  preferences.putUInt("memory_pos", memory_pos);
  preferences.putUInt("memory_count", memory_count);

  uint32_t seqNum[lenPrefMemory] = {0}; // reset latitude
  preferences.putBytes("seqNum", seqNum, lenPrefMemory*sizeof(uint32_t));

  float latitude[lenPrefMemory] = {0}; // reset latitude
  preferences.putBytes("latitude", latitude, lenPrefMemory*sizeof(float));

  float longitude[lenPrefMemory] = {0}; // reset longitude
  preferences.putBytes("longitude", longitude, lenPrefMemory*sizeof(float));
}

void flash_memory::save_data(uint32_t seqNum, float lat_data, float long_data)
{
  Serial.println(" -- Save memory --");
  unsigned int memory_pos = preferences.getUInt("memory_pos", 0);

  size_t seqNumLen = preferences.getBytesLength("seqNum");
  uint32_t bufferSeqNum[seqNumLen];
  preferences.getBytes("seqNum", bufferSeqNum, seqNumLen);
  bufferSeqNum[memory_pos] = seqNum;
  preferences.putBytes("seqNum", bufferSeqNum, lenPrefMemory * sizeof(uint32_t));
  
  size_t latLen = preferences.getBytesLength("latitude");
  float bufferLat[latLen];
  preferences.getBytes("latitude", bufferLat, latLen);
  bufferLat[memory_pos] = lat_data;
  preferences.putBytes("latitude", bufferLat, lenPrefMemory * sizeof(float));

  size_t longLen = preferences.getBytesLength("longitude");
  float bufferLong[longLen];
  preferences.getBytes("longitude", bufferLong, longLen);
  bufferLong[memory_pos] = long_data;
  preferences.putBytes("longitude", bufferLong, lenPrefMemory * sizeof(float));

  if (memory_pos < lenPrefMemory) {
    memory_pos++;
  } else {
    memory_pos = 0;
  }

  Serial.printf(" Current counter value: %u\n", memory_pos);
  preferences.putUInt("memory_pos", memory_pos);
}


void flash_memory::print_memory()
{
  Serial.println(" -- Print memory --");

  unsigned int memory_pos = preferences.getUInt("memory_pos", 0);
  unsigned int memory_count = preferences.getUInt("memory_count", 0);

  Serial.printf("\n Última posição registrada: %d \n", memory_pos);
  Serial.printf("\n Número de posições registradas: %d \n", memory_count);

  // Mostra o que tem armazenado no vetor de seqNum
  Serial.printf("\n SeqNum: \n");
  size_t seqNumLen = preferences.getBytesLength("seqNum");
  uint32_t bufferSeqNum[seqNumLen];
  preferences.getBytes("seqNum", bufferSeqNum, seqNumLen);
  Serial.printf("Size seqNum len %d \n", seqNumLen);

  int pos;
  uint32_t limitVect = sizeof(bufferSeqNum);
  Serial.printf("Size of vect seqNum %d ", limitVect);
  for (pos = 0; pos < lenPrefMemory; pos++) {
    Serial.printf("Pos: %d ", pos);
    Serial.printf("Data: %d ", bufferSeqNum[pos]);
  }
  
  // Mostra o que tem armazenado no vetor de latitude
  Serial.printf("\n Latitude: \n");
  size_t latLen = preferences.getBytesLength("latitude");
  float bufferLat[latLen];
  preferences.getBytes("latitude", bufferLat, latLen);
  Serial.printf("Size lat len %d \n", latLen);

  limitVect = sizeof(bufferLat);
  Serial.printf("Size of vect lat %d ", limitVect);
  for (pos = 0; pos < lenPrefMemory; pos++) {
    Serial.printf("Pos: %d ", pos);
    Serial.printf("Data: %f ", bufferLat[pos]);
  }

  // Mostra o que tem armazenado no vetor de longitude
  Serial.printf("\n Longitude: \n");
  size_t longLen = preferences.getBytesLength("longitude");
  float bufferLong[longLen];
  preferences.getBytes("longitude", bufferLong, longLen);

  Serial.printf("Size long len %d \n", latLen);
  limitVect = sizeof(bufferLong);
  Serial.printf("Size of vect lat %d ", limitVect);
  for (pos = 0; pos < lenPrefMemory; pos++) {
    Serial.printf("Pos: %d ", pos);
    Serial.printf("Data: %f ", bufferLong[pos]);
  }
  Serial.printf("\n");
}
