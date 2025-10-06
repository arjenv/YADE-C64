#include "main.h"

int get_indexdata(char* output, int arraysize) {
  JsonDocument json_indexdata;
  //int debuglength;
  if (!strlen(Notification)) strcpy(Notification, "done");

  json_indexdata[0]["name"] = "C64filename";
  json_indexdata[0]["value"] = C64filename;
  json_indexdata[1]["name"] = "Uploadformat";
  json_indexdata[1]["value"] = Uploadformat;
  json_indexdata[2]["name"] = "Notification";
  json_indexdata[2]["value"] = Notification;

//  debuglength = serializeJson(json_indexdata, output, arraysize);
  serializeJson(json_indexdata, output, arraysize);
  // time critical...   if (PO) tee.printf("Get_indexdata: %s\n", output);
  return(strlen(output));        
}

bool Readconfigfile() {

  File configFile;
  size_t ConfigFile_size;
  JsonDocument json;

  if (LittleFS.exists("/config.json")) { //file exists, then reading and loading
    tee.printf("reading config file\n");
    configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      tee.printf("opened config file\n");
      ConfigFile_size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[ConfigFile_size]);

      configFile.readBytes(buf.get(), ConfigFile_size);
      auto deserializeError = deserializeJson(json, buf.get());
      serializeJson(json, Serial);
      if (!deserializeError) {
        tee.printf("\nparsed json\n");
        if(json["ip"]) {
          tee.printf("setting custom ip from config\n");
          strncpy(static_ip, json["ip"], 16);
          strncpy(static_gw, json["gateway"], 16);
          strncpy(static_sn, json["subnet"], 16);
          strncpy(ssid, json["ssid"], 30);
          strncpy(password, json["pass"], 30);
          tee.printf("%s\n", static_ip);
        } 
        else {
          tee.printf("no custom ip in config\n");
          return(false);
        }
      } 
      else {
        tee.printf("deserialization error\n");
        return(false);
      }
    }
    else {
      tee.printf("Failed to open /config.json\n");
      return(false);
    }
  }
  else {
    tee.printf("/config.json does not exist\n");
    return(false);
  }
  //end read. success!
  tee.printf("%s\n", static_ip);
  return(true);
}

bool   ReadSettings(void) {  // get the variables from file.

  File      settingsFile;
  size_t    SettingsFileSize;
  JsonDocument json;


  if (LittleFS.exists("/settings.json")) { //file exists, then reading and loading
    tee.printf("reading settings file\n");
    settingsFile = LittleFS.open("/settings.json", "r");
    if (settingsFile) {
      tee.printf("opened settings.json file\n");
      SettingsFileSize = settingsFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[SettingsFileSize]);

      settingsFile.readBytes(buf.get(), SettingsFileSize);
      auto deserializeError = deserializeJson(json, buf.get());
      serializeJson(json, Serial);
      if (!deserializeError) {
        tee.printf("\nparsed settings.json\n");
        CBMsyncLeader = json["CBMsyncLeader"]; tee.printf("CBMsyncLeader: %i\n", CBMsyncLeader);
        TurbosyncLeader = json["TurbosyncLeader"]; tee.printf("TurbosyncLeader: %i\n", TurbosyncLeader);
        TurboThreshold = json["TurboThreshold"]; tee.printf("TurboThreshold: %i\n", TurboThreshold);
        TurboPilotebyte = json["TurboPilotebyte"]; tee.printf("TurboPilotebyte: %i\n", TurboPilotebyte);
      } 
      else {
        tee.printf("deserialization error\n");
        return(false);
      }
    }
    else {
      tee.printf("Failed to open /settings.json\n");
      return(false);
    }
  }
  else {
    tee.printf("/settings.json does not exist\n");
    return(false);
  }
  //end read. success!
  tee.printf("Successfully read settings.json\n");
  return(true);
}

void StoreSettings(void) {
  //save the custom variables to FS
  tee.println("saving settings.json");
  JsonDocument json;
  json["CBMsyncLeader"] = CBMsyncLeader;
  json["TurbosyncLeader"] = TurbosyncLeader;
  json["TurboThreshold"] = TurboThreshold;
  json["TurboPilotebyte"] = TurboPilotebyte;

  File configFile = LittleFS.open("/settings.json", "w");
  if (!configFile) {
    tee.println("failed to open settings file for writing");
  }

  serializeJson(json, tee);
  serializeJson(json, configFile);
  tee.println();
  configFile.close();
  //end save
}

uint8_t get_extension(char* C64Basename, char* C64filename) {
  // Test C64Filename for its extension
  // Returns 1 if .PRG, 2 if .TAP else 0
  // Fills char* C64Basename with the base of the filename (uppercase). e.g. /prg/Somefile.prg --> C64Basename = 'SOMEFILE', returns 1 (prg)
  // check C64basename length max = 16 characters (crop length)

  char *ptr, ext[4];
  int   extension = 0;

  if(PO) tee.printf("Get_extension: Full path: %s\n", C64filename);

  ptr = strchr(C64filename, '.'); // locate last occurrence of '.'
  strncpy(ext, ptr+1, 3);
  ext[3] = 0;
  strupr(ext);
  if (!strcmp(ext, "PRG")) extension = 1;
  if (!strcmp(ext, "TAP")) extension = 2;

  if (PO) tee.printf("Get_extension --> %s, %i\n", ext, extension);

  ptr = strrchr(C64filename, '/'); // locate last occurrence of '/'
  tee.printf("get_extension: ptr= %s length= %i\n", ptr, strlen(ptr));
  if (strlen(ptr) < 21){
    strncpy(C64Basename, ptr+1, strlen(ptr)-5);
    C64Basename[strlen(ptr)-5] = 0;
  }
  else {
    strncpy(C64Basename, ptr+1, 16);
    C64Basename[16] = 0;
  }
  strupr(C64Basename); // commodore needs uppercase
  if (PO) tee.printf("Get_extension --> C64basename:\t%s\n", C64Basename);
  return(extension); // return zero is fault.
}

bool fill_header(File C64PRG, char *Bname, char *header) {

  uint8_t   startaddressLowbyte, startaddressHighbyte, endaddressLowbyte, endaddressHighbyte;	
  uint8_t   headerchecksum, i, turboT;
  uint16_t  startaddress, endaddress;
  File      YADE_header;

  tee.printf("fill_header: Bname = %s\n", Bname);
  startaddressLowbyte = C64PRG.read(); // 1st byte
 	startaddressHighbyte = C64PRG.read(); // 2nd byte
 	startaddress = (startaddressHighbyte<<8) + startaddressLowbyte;
 	if (PO) tee.printf("Startaddress:\t$%02X%02X --> %i\n", startaddressHighbyte, startaddressLowbyte, startaddress);
	endaddress = startaddress + C64PRG.size()-2;
	if (PO) tee.printf("Endaddress:\t%i --> %04x\n", endaddress, endaddress);
	endaddressLowbyte = endaddress%256;
	endaddressHighbyte = (endaddress>>8);
 	if (PO) tee.printf("Endaddress:\t$%02X%02X --> %i\n", endaddressHighbyte, endaddressLowbyte, endaddress);

	// Fill the headerblock
	
  if (Uploadformat==0 || Uploadformat == 2) {
    header[0] = 0x03; // file type non relocatable prg
    turboT=0;
  }
  else {
    header[0] = 0x02;
    turboT=1;
    header[5] = 0x00; // required with Turbospeed. Byte not understood.
  }
	header[1] = startaddressLowbyte; // startaddress low byte
	header[2] = startaddressHighbyte; // startaddres high byte
	header[3] = endaddressLowbyte; // endaddress low byte
	header[4] = endaddressHighbyte; // endaddress High byte

	// header 1-4 already filled with start/end address
	for (i=0; i<16; i++) { //fill filename 
		if (i < strlen(Bname)) {
			header[i+5+turboT] = (uint8_t) Bname[i];
		}
		else {
			header[i+5+turboT] = 0x20; // space
		}
	}

  // space for extra character or a small program:
  if (Uploadformat == 2) { //autoturbo fast
    YADE_header = LittleFS.open("/YADE_headerset2B2.bin", "r");
    if (!YADE_header) {
      tee.printf("Cannot open /YADE_header.bin\n");
      return(1); //error
	  }
  	for (i=0; i<YADE_header.size(); i++) {
	  	header[i+5+turboT+16] = YADE_header.read();
		  if (header[i+5+turboT+16] == 0xFE) {
			  tee.printf("Found FE: %i, %X\n", i+5+16, header[i+5+turboT+16]);
  			header[i+5+turboT+16] = int(TurboThreshold);
	  		tee.printf("New Threshold: %i (%X)\n", header[i+5+turboT+16], header[i+5+turboT+16]);
		  	tee.printf("%i -- %i -- %i\n", int(TurboThreshold/8.0 + 0.5) - 4, int(TurboThreshold/8), int(TurboThreshold/8.0 + 0.5) + 4);
  		}
	  }
  	for (i=YADE_header.size(); i<171; i++) { //fill the rest of the header
	  	header[i+5+turboT+16] = 0x20; //spaces
  	}
	  YADE_header.close(); 
  }
  else {
	  for (i=0; i<171; i++) { //fill the rest of the header
		  header[i+5+turboT+16] = 0x20; //spaces
    }
	}

	// checksum
	headerchecksum = 0;
	for (i=0; i<192; i++) {
		headerchecksum^=header[i];
		//tee.printf("%i\t%02x\n", i, header[i]);
	}
	header[192] = headerchecksum;
	if (PO) tee.printf("Header checksum:\t%02X\n", headerchecksum);
  return(0);
}

void  Send_remaining_CB(void) { // Keep outputting CircularBuffer data until motor halts
	time_t  timeout=millis();
  bool timeoutflag = false;

  //	while (!timeoutflag && cb.count && digitalRead(TAPEMOTOR)) { //sync sequence short
	while (!timeoutflag && digitalRead(TAPEMOTOR)) { //sync sequence short
		if ((millis()-timeout) >2000) {
			tee.printf("\nTimeout closing pulses..");
			timeoutflag = 1;
		}
		if (PO) Serial.printf("Sending pulses from buffer until motor stops.... %i\n", cb.count);
	}
}



char *strInsert(char *str1, const char *str2, uint8_t pos) {
    size_t l1 = strlen(str1);
    size_t l2 = strlen(str2);

    if (pos > l1) pos = l1;

    char *p  = str1 + pos;
    memmove(p + l2, p, l1 - pos + 1);
    memcpy (p, str2,  l2);
    return str1;
}

