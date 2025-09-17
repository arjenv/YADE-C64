/*********************************************************
 * 
 * Arjen Vellekoop
 * 30 june 2025
 * C64 YADE (Yet Another Datasette Emulator)
 * 
 * ESP8266
 * 
 * 30 June 2025 Add OTA support.
 * 30 june 2025 Add telnet 
 * 30 june 2025 add 'tee.print' print to both Serial as Telnet.
 * 
 * 3 july 2025 add ringbuffer functions
 * 
 * 4 july 2025 add normal speed TAP
 * 4 july 2025 add timer1 interrupt
 * 
 * This version is proof of concept. It reads a fixed .tap file and 
 * uploads it to C64 in normal speed.
 * 
 * 9 july 2025 changed timer1 from DIV1 to DIV16. =200nsec
 * 
 * 11 july 2025 Changed timer1 interrupt routine. 
 *    Tperiods now pushed each High and Low
 * 
 * 13 july 2025 Start in /prg
 *    and warning if entering root
 *    When byte=0x00 calculate silence gap and push to ringbuffer
 *    Changed ringbuffer from uint8_t to uint32_t
 * 
 * add .PRG functionality
 * add autoturbo
 * 
 *  23 August 2025 add wifimanager static ip
 * *
 * *
 * 
 *  28 August
 *    added variables 
 *    added settings.html
 *    added settings.json
 * 
 *    this version works with auto turbo
 * 
 *  17 september. 
 *    Clean up code 
 * 
 * 
 *   
 * 
 * 
 * 
 * 
 * 
 *  
 ***********************************************************/

#include "main.h"




//Global variables

File              C64PRG, YADE_FL;
char              c64prgfilename[50];
char              PRGbyte;
uint32_t          nrofbytes;
volatile          RingBuffer cb;
const int         ledPin = 2;
volatile uint32   Tperiod;
volatile uint8_t  interrupt_occured=0, errorflag=0;
volatile bool     ReadOutput=1, first_interrupt;
bool              former_motorstate, current_motorstate;
char              C64filename[100];
uint8_t           Uploadformat; // 0=normal speed, 1=turbo speed, 2=AutoTurbo
char              variable_chars[800];
bool              playpressed;
uint8_t           TapeState;
bool              panicbutton;
time_t            timeout;
bool              timeoutflag;
uint32_t          longsilence;
uint8_t           extension;
bool              SetupFinished;
bool              flag_setvalues=false;
uint8_t           error_nr=0;
char 	            *ptr, C64basename[30], header[200];
char              Notification[100];

// variables for auto turbo
uint16_t          CBMsyncLeader = 0x6A00;
uint16_t          TurbosyncLeader = 1271, TurboPilotebyte=503;
uint8_t           TurboThreshold=0xFE;

AsyncWebServer server(80);
TeePrint tee(Serial, TelnetStream); // stream print(f) statements to both Serial as Telnet. 

void IRAM_ATTR timer1ISR() {
  Tperiod = 5*4*cb.buffer[cb.tail]; // actually 5*4*Tperiod/0.9852486 for PAL (1.0227273 MHz for NTSC vs. 0.9852486 MHz for PAL)

  if (cb.count && digitalRead(TAPEMOTOR)) {
    timer1_write(Tperiod); 
    cb.tail = (cb.tail + 1) % BUFFER_SIZE;
    cb.count--;
    digitalWrite(TAPEREAD, !ReadOutput);
  //  digitalWrite(ledPin, !digitalRead(ledPin));
    ReadOutput = !ReadOutput;
  }
  if (!digitalRead(TAPEMOTOR)) {
    Serial.printf("ISR Motor off... %i\n", timer1_read());
  }
  if (!cb.count) { // debug, should never happen
    Serial.printf("Interrupt... cbcount = 0\n");
    Serial.printf("Tail, head: %i, %i \n\n", cb.tail, cb.head);
    errorflag = 1;
  }

  interrupt_occured++; // debug should be rare...
}



char static_ip[16] = "192.168.1.11";
char static_gw[16] = "192.168.1.254";
char static_sn[16] = "255.255.255.0";
char  ssid[30];
char  password[30];

/***********  SETUP       ******************/
void setup() {

  Serial.begin(115200);
  server.begin();
  TelnetStream.begin();  
  tee.printf("\n\nVersion: %s\n", Version);
  
  //******************  wifi setup  *******************

  //read configuration from FS json

  if (LittleFS.begin()) {
    if (Readconfigfile()) { // does /config.json exists?
      // yes try and connect
      if (!initWiFi()) {
        tee.printf("Failed to init Wifi, starting WiFi Manager as Access Point (AP).\nConnect to 192.168.4.1\n");
        SetupWifiManager();
      }
    }  
    else {
      SetupWifiManager();
    }
  }
  else {
    tee.printf("Could not read Flash..  Abort\n");
    exit(1);
  } 

  tee.println("local ip");
  tee.println(WiFi.localIP());
  tee.println(WiFi.gatewayIP());
  tee.println(WiFi.subnetMask());


// *********************end wifi setup ********************


  configureFiles();
  configureTAR();
  InitArduinoOTA();
  reset_RingBuffer(&cb);
  ReadSettings();  // get the variables from file.

  // route to index.html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false);
  });
  server.on("/get_indexdata", HTTP_GET, [](AsyncWebServerRequest *request){
    get_indexdata(variable_chars, sizeof(variable_chars)); // we need the sizeof, as in the function it will loose its value
    request->send_P(200, "application/json", variable_chars);
  });
  server.on("/selectfile", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    //Serial.println(logmessage);
    //char fileName[100];
    if (request->hasParam("name")) {
      const char *fileName = request->getParam("name")->value().c_str();

      logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + "?name=" + String(fileName);

      if (!LittleFS.exists(fileName)) {
        tee.println(logmessage + " ERROR: file does not exist");
        request->send(400, "text/plain", "ERROR: file does not exist");
      } 
      else {
        tee.println(logmessage + " file exists");
        strcpy(C64filename, fileName);
        request->send(200, "text/plain", "Selected File: " + String(fileName));
      }
    }
    else {
      request->send(400, "text/plain", "ERROR: \'name\' parameter required");
    }
  });
  server.on("/playbutton", HTTP_GET, [](AsyncWebServerRequest * request) {
    strcpy(C64filename, request->getParam("C64filename")->value().c_str());
    if(request->hasParam("Uploadformat")) {
      Uploadformat = request->getParam("Uploadformat")->value().toInt();
      strcpy(Notification, "Converting .PRG to datasette format.");
    }
    else {
      Uploadformat = 10;
      strcpy(Notification, "Converting .TAP to datasette format.");
    }
    if (!strcmp(request->getParam("play")->value().c_str(), "Save")) {
        playpressed = false;
        panicbutton = false;
    }
    else if (!strcmp(request->getParam("play")->value().c_str(), "PLAY")) {
        playpressed = true;
        panicbutton = false;
    }
    else if (!strcmp(request->getParam("play")->value().c_str(), "PANIC")) {
        playpressed = false;
        panicbutton = true;
    }


    tee.printf("Playbutton:\n");
    tee.printf("C64filename: %s\n", C64filename);
    tee.printf("Uploadformat: %i\n", Uploadformat);
    tee.printf("playpressed: %i\n", playpressed);
    tee.printf("panicbutton: %i\n", panicbutton);
    request->redirect("/");

  });
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/settings.html", "html", false);
  });  
  // Route to load style.css file
  server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/css/style.css", "text/css");
  });
  server.on("/get_settings", HTTP_GET, [](AsyncWebServerRequest *request){
//    get_settings(variable_chars, sizeof(variable_chars)); // we need the sizeof, as in the function it will loose its value
    request->send(LittleFS, "/settings.json", "application/json");
  });

// Send a GET request to <ESP_IP>/get?inputString=<inputMessage>
// 27 August 2025
// Cannot use StoreValues directly, it triggers a Soft WDT
// Instead a flag is set: flag_setvalues = true;

server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) { // receive and store variables
    uint32_t inputMessage = 0;
    // GET inputString value on <ESP_IP>/get?inputString=<inputMessage>
    if (request->hasParam("CBMsyncLeader")) {
      inputMessage = request->getParam("CBMsyncLeader")->value().toInt();
      tee.printf("Received new value CBMsyncLeader: %i\n\n", inputMessage);
      CBMsyncLeader = inputMessage;
      flag_setvalues = true;
    }    
    if (request->hasParam("TurbosyncLeader")) {
      inputMessage = request->getParam("TurbosyncLeader")->value().toInt();
      tee.printf("Received new value TurbosyncLeader: %i\n\n", inputMessage);
      TurbosyncLeader = inputMessage;
      flag_setvalues = true;
    }
    if (request->hasParam("TurboThreshold")) {
      inputMessage = request->getParam("TurboThreshold")->value().toInt();
      tee.printf("Received new value TurboThreshold: %i\n\n", inputMessage);
      TurboThreshold = inputMessage;
      flag_setvalues = true;
    }
    else if (request->hasParam("TurboPilotebyte")) {
      inputMessage = request->getParam("TurboPilotebyte")->value().toInt();
      tee.printf("Received new value Used today: %i\n\n", inputMessage);
      TurboPilotebyte = inputMessage;
      flag_setvalues = true;
    }    
    else {
      tee.println("No message sent");
    }
    char returnmsg[10];
    itoa(inputMessage, returnmsg, 10);
    tee.printf("returning 200 %s\n", returnmsg);
    request->send(200, "text/plain", returnmsg); // not sure if this is required but shuts up browser..
  });

  // Interrupt stuff..

    pinMode(ledPin, OUTPUT); //LED
    pinMode(TAPEMOTOR, INPUT);
    pinMode(TAPEREAD, OUTPUT);
    pinMode(TAPEWRITE, INPUT);
    pinMode(TAPESENSE, OUTPUT);
    timer1_attachInterrupt(timer1ISR); 
   
    /* Prescalers:
        TIM_DIV1      80MHz => 80 ticks/µs => Max: (2^23 / 80) µs =  ~0.105s
        TIM_DIV16     5MHz  => 5 ticks/µs => Max: (2^23 / 5) µs = ~1.678s
        TIM_DIV256    0.3125MHz => 0.3125 ticks/µs => Max: (2^23 / 0,3125) µs = ~26.8s
      Interrupt TYPE:
        TIM_EDGE      no other choice here
      Repeat?:
        TIM_SINGLE  0 => one time interrupt, you need another timer1_write(ticks); to restart
        TIM_LOOP    1 => regular interrupt 
    */
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
    digitalWrite(TAPESENSE, 1); // no keys pressed

    strcpy(C64filename, "Choose file -->");
    Uploadformat = 2; // 0=normal speed, 1= Turbo speed, 2=AutoTurbo, 3=Autoturbofast
    TapeState = STATE_IDLE;
    tee.printf("End Setup. Starting loop\n\n");

}

void loop () {

  ArduinoOTA.handle();
  HandleTelnet();
  if (interrupt_occured) { //interrupt outside the while loop?
    tee.printf("Interrupt occured: %i\n", interrupt_occured);
    interrupt_occured--;
  }

  if (flag_setvalues) { // values were changed manually
    StoreSettings();
    flag_setvalues = false;
  }

  if (!digitalRead(FLASHBUTTON) || playpressed) { //pressed playbutton
    tee.printf("\nPressed Play\n");
    while (!digitalRead(FLASHBUTTON)); // debounce
    TapeState = STATE_IDLE;
    if (!(extension = get_extension(C64basename, C64filename))) error_nr = 1; // is it TAP or PRG? C64Filename required
    if (!(C64PRG = LittleFS.open(C64filename, "r"))) {
      tee.printf("Cannot open this file %s\n", C64filename);
      error_nr=2;
      TapeState = STATE_FAULT;
    }
    else {
      tee.printf("opened C64PRG File %s\n", C64filename);
    }

    playpressed = false; // reset
    panicbutton = false;
    digitalWrite(TAPEREAD, 0);
    digitalWrite(TAPESENSE, 0); // same as pressing play for the C64

    if (!error_nr) {
      tee.printf("Filesize: %i\n", C64PRG.size());

      nrofbytes = 0;
      timeout = millis();
      timeoutflag = false;
      current_motorstate = false;
      while (!timeoutflag && !digitalRead(TAPEMOTOR)) { // wait for motor to come up or timeout
        if ((millis() - timeout) > 1000) {
          tee.printf("Motor voltage did not engage\nAborting\n\n");
          TapeState = STATE_FAULT;
          timeoutflag = true;
          error_nr=4;
        }
      }
      if (!timeoutflag) { //Ok motor voltage is high AND within timeout AND file exists
        current_motorstate = true;
        TapeState = STATE_HEADER;
        ReadOutput = 1;
        errorflag = 0;
        //timer1_running = 0;
        first_interrupt = false;
      }
    }
    if ((extension == 1) && (!error_nr)) { // .PRG

      // fill the header
      if (Uploadformat != 2) {
        fill_header(C64PRG, C64basename, header);
      }
      else { // open YADE_FL.prg and fill header
        	YADE_FL = LittleFS.open("/YADE_FL.prg", "r");
          if (!YADE_FL) {
            tee.printf("Cannot open /YADE_FL.prg\n");
            // need some error code here to skip rest
	        }
          else {
            fill_header(YADE_FL, C64basename, header);
            YADE_FL.close();
          }
      }
      //C64PRG.seek(0, SeekSet); // REQUIRED NOW. DELETE LATER !!!
      switch (Uploadformat) {
        case 0: // Normal speed
          prg2tap(C64PRG, header);
          break;
        case 1: // Turbo speed
          prg2turbo(C64PRG, header);
          break;
        case 2: // Autoload with turbo speed
          prg2turbo_auto(C64PRG, header);
          break;
        default: // Normal speed
          if (PO) tee.printf("Unexpected value of Uploadformat: %i\n", Uploadformat);
          break;

      }

    }
    if (extension == 2) { //Tap
      TAP2CBMtape(C64PRG, TapeState, panicbutton);
    }

    // cleaning up, common for all modes
    if (PO) tee.printf("Closing file %s\n", C64filename);
    C64PRG.close();
    interrupt_occured = 0;
    if (PO) tee.printf("Resetting ringbuffer\n");
    reset_RingBuffer(&cb);
	  if (PO) tee.printf("Pressing stop on taperecorder...\n");
	  digitalWrite(TAPESENSE, 1);


  }
}
