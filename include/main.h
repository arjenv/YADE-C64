#ifndef MYMAIN_H
#define MYMAIN_H

/* include all headers whose types you need */

#include <Arduino.h>
#include "FSsystem.h"
#include "create_tar.h"
#include "FS.h"
#include <LittleFS.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <TelnetStream.h>
#include "TeePrint.h" 
#include <ArduinoOTA.h>
#include "circularbuffer.h"

/* define macros */

#define Version         "C64 YADE V1.1 2 October 2025"
#define httpPort        80
#define FLASHBUTTON     0 //GPIO0
#define TAPESENSE       13      // D7
#define TAPEWRITE       12      // D6
#define TAPEREAD        14      // D5
#define TAPEMOTOR       5       // D1

#define STATE_IDLE      1
#define STATE_HEADER    2
#define STATE_PROGRAM   3
#define STATE_FAULT     4

#define PO              1 // Debug PO=printer output

// CBM tape length values 
#define CBMPULSE_SHORT  0x2F  	// 47
#define CBMPULSE_MEDIUM 0x42	// 66
#define CBMPULSE_LONG   0x56	// 86
#define	TURBO_SHORT	    0x1A
#define	TURBO_LONG	    0x28


/* declare global variables */

extern char                 C64filename[100];
extern uint8_t              Uploadformat;
extern time_t               timeout;
extern bool                 timeoutflag;
extern volatile RingBuffer  cb;
extern char                 PRGbyte;
extern volatile bool        ReadOutput;
extern volatile uint8_t     interrupt_occured;
extern char                 ssid[30];
extern char                 password[30];
extern char                 static_ip[16];
extern char                 static_gw[16];
extern char                 static_sn[16];
extern bool                 SetupFinished;
extern uint16_t             CBMsyncLeader, TurbosyncLeader, TurboPilotebyte;
extern uint8_t              TurboThreshold;
extern  char                Notification[100];

/* declare global functions */

void        InitArduinoOTA(void);
void        HandleTelnet(void);
void        reset_RingBuffer(volatile struct RingBuffer *);
bool        isFull(volatile struct RingBuffer *);
bool        isEmpty(volatile struct RingBuffer *);
void        push_ringbuffer(volatile struct RingBuffer *, uint8_t);
void        push_ringbuffer_twice(volatile struct RingBuffer *, uint8_t);
uint8_t     pull_ringbuffer(volatile struct RingBuffer *);
int         get_indexdata(char*, int);
void        prg2tap(File C64PRG, char *header);
void        prg2turbo(File C64PRG, char *header);
void        prg2turbo_auto(File C64PRG, char *header);
bool        initWiFi(void);
bool        Readconfigfile(void);
bool        SetupWifiManager(void);
bool        ReadSettings(void);  // get the variables from file.
void        StoreSettings(void);
void        byte2cbm(uint8_t * bitstring, uint8_t byte); // can be deleted later?
uint8_t     get_extension(char* C64basename, char* C64filename);
bool        fill_header(File C64PRG, char* C64Basename, char* header);
void        TAP2CBMtape(File C64PRG, uint8_t TapeState, bool panicbutton);
void        Send_remaining_CB(void); // Keep outputting CircularBuffer data until motor halts
char        *strInsert(char *str1, const char *str2, uint8_t pos);
int         SaveTAP(bool cleanUpTAP, bool panicbutton, bool RECOverwrite);
void        IRAM_ATTR timer1ISR(void);
void        IRAM_ATTR ISRSave();

#endif