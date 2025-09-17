#ifndef CREATE_TAR_H
#define CREATE_TAR_H

#include <Arduino.h>
#include "create_tar.h"
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

void configureTAR(void);
void create_TAR(char * , File , File );
void scan_dir(char * , File );



extern AsyncWebServer server;


#endif