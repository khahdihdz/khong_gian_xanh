#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

void otaManagerInit();
void otaManagerLoop();
String otaManagerGetStatus();
String otaManagerGetInfoJson();
bool otaManagerCheckNow();
bool otaManagerUpdateNow();

#endif
