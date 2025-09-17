#ifndef FSSYSTEM_H
#define FSSYSTEM_H

#include <Arduino.h>
#include "FSsystem.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson


void configureFiles(void);
String listAllFilesInDirjson(const char *);
String calculate_memory(void);
void handleUpload(AsyncWebServerRequest *, String , size_t , uint8_t *, size_t , bool );
void notFound(AsyncWebServerRequest *);


extern AsyncWebServer server;

#endif