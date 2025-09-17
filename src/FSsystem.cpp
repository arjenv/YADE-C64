// File Adminstration for LittleFS files ESP8266
// %PARAM% is handled by the processor Espasyncwebserver library
// Author: Arjen Vellekoop
// July 2024

#include "main.h"

String FSprocessor(const String& var) {
  FSInfo fs_info_webserver;
  LittleFS.info(fs_info_webserver);
  if (var == "FIRMWARE") {
    return Version;
  }
  return("Error .. ARV"); // should never return this but avoids error/warning
}

void configureFiles() {
  // configure web server

  // if url isn't found
  server.onNotFound(notFound);

  // run handleUpload function when any file is uploaded
  server.onFileUpload(handleUpload);

  // Route to load style.css file
  server.on("/css/admin.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/css/admin.css", "text/css");
  });

  // Routes to load images
  server.on("/images/icons8-home-40.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-home-40.png", "images/png");
  });   
  server.on("/images/icons8-add-folder-48.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-add-folder-48.png", "images/png");
  });
  server.on("/images/icons8-upload2-40.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-upload2-40.png", "images/png");
  });   
  server.on("/images/icons8-tar2-40.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-tar2-40.png", "images/png");
  });   
  server.on("/images/icons8-download2-25.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-download2-25.png", "images/png");
  });   
  server.on("/images/icons8-delete-25.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-delete-25.png", "images/png");
  });   
  server.on("/images/icons8-crayon-30.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-crayon-30.png", "images/png");
  });
  server.on("/images/icons8-folder-40.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/images/icons8-folder-40.png", "images/png");
  });   


  server.on("/files", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(LittleFS, "/files.html", "text/html", false, FSprocessor);
  });
  
  server.on("/memory", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(200, "text/plain", calculate_memory());
  });

  server.on("/listfiles", HTTP_GET, [](AsyncWebServerRequest * request) {
    const AsyncWebParameter* p = request->getParam("listdir");
    //Serial.print("in listfiles: P-value= ");Serial.print(p->value());Serial.println();
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    //Serial.println(logmessage);

    request->send(200, "text/plain", listAllFilesInDirjson(p->value().c_str()));
  });

  server.on("/makedirectory", HTTP_GET, [](AsyncWebServerRequest * request) {
    
    char dir2make[50];
    const AsyncWebParameter* p = request->getParam("newdir");
    Serial.print("in makedirectory: P-value= ");Serial.print(p->value());Serial.println();
    p->value().toCharArray(dir2make,49);

    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    if (LittleFS.exists(dir2make)) {
      request->send(403, "text/plain", "Could not create: duplicate "+p->value());
    }
    if (LittleFS.mkdir(p->value())) { //success
      request->send(200, "text/plain", "created "+p->value());
    }
    else {
      request->send(400, "text/plain", "Could not create "+p->value());
    }
  });


  server.on("/file", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    //Serial.println(logmessage);
    //char fileName[100];
    if (request->hasParam("name") && request->hasParam("action")) {
      const char *fileName = request->getParam("name")->value().c_str();
      const char *fileAction = request->getParam("action")->value().c_str();
      const char *fileReName = request->getParam("rename")->value().c_str();

      logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + "?name=" + String(fileName) + "&action=" + String(fileAction);

      if (!LittleFS.exists(fileName)) {
        Serial.println(logmessage + " ERROR: file does not exist");
        request->send(400, "text/plain", "ERROR: file does not exist");
      } 
      else {
        tee.println(logmessage + " file exists");
        if (strcmp(fileAction, "download") == 0) {
          logmessage += " downloaded";
          Serial.println();Serial.println(fileName);Serial.println();
          request->send(LittleFS, fileName, "application/octet-stream");
        } 
        else if (strcmp(fileAction, "delete") == 0) {
          logmessage += " deleted";
          LittleFS.remove(fileName);
          request->send(200, "text/plain", "Deleted File: " + String(fileName));
        }
        else if (strcmp(fileAction, "rename") == 0) {
          logmessage += " rename";
          LittleFS.rename(fileName, fileReName);
          request->send(200, "text/plain", "Renamed File: " + String(fileName) + "to" + String(fileReName));
        }
        else {
          logmessage += " ERROR: invalid action param supplied";
          request->send(400, "text/plain", "ERROR: invalid action param supplied");
        }
        Serial.println(logmessage);
      }
    }
    else {
      request->send(400, "text/plain", "ERROR: name and action params required");
    }
  });
}

void notFound(AsyncWebServerRequest *request) {
  String logmessage = "notFound Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);
  int paramsNr = request->params();
//  Serial.printf("Nrofparamters: %i\n", paramsNr);

  int i;
  for(i=0;i<paramsNr;i++){
    const AsyncWebParameter* p = request->getParam(i);
    Serial.println(p->value());
    }
}

// handles uploads to the fileserver
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  // make sure authenticated before allowing upload
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);

  if (!index) {
      logmessage = "Upload Start: " + request->url() + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = LittleFS.open(request->url() + filename, "w");
    Serial.println(logmessage);
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + request->url() + String(filename) + " index=" + String(index) + " len=" + String(len);
    Serial.println(logmessage);
  }

  if (final) {
    logmessage = "Upload Complete: " + request->url() + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    Serial.println(logmessage);
    request->redirect("/files");
  }
}

String calculate_memory(void) {
  FSInfo fs_info_webserver;
  LittleFS.info(fs_info_webserver);
  String cmem="{\"littlefs_total\":" + (String)fs_info_webserver.totalBytes + ", \"littlefs_used\":" + (String)fs_info_webserver.usedBytes + "}";
  //Serial.println(cmem);
  return (cmem);
}


//******************************************************************************** 
String listAllFilesInDirjson(const char *directory) {
  /********************************************************************************************************
   * 
   *   fill json string with filenames and directories.
   *   
   *     format: "name":"(filename||dir)","type":"(file||dir||full)","size":(size||dir)","lastmod":"dd/mm/yy"
   *     
   *     If it is a drectory "size" = 0 
   *     When going down in directory structure add "name":"../" as to go up again when required.
   *     function fills 'responsestring' as jsonstring
   *     
   *     Arjen Vellekoop
   *     July 19, 2024
   *     
   *     
   ********************************************************************************************************/
  
  Dir dir = LittleFS.openDir(directory);
  time_t filedate;
  struct tm filecreation;
  JsonDocument json_filelist;
  int filecount=0;
  String responsestring;
  
  //Serial.printf("ListallfilesInDirjson: directory = %s %d\n", directory, strlen(directory));
  json_filelist[filecount]["name"] = directory;
  json_filelist[filecount]["type"] = "full";
  json_filelist[filecount]["size"] = 0;
  json_filelist[filecount++]["lastmod"] = "";

  while(dir.next()) {
    filedate = dir.fileCreationTime();
    localtime_r(&filedate, &filecreation);           // update the structure tm with the current time
    json_filelist[filecount]["name"] = dir.fileName();

    if (dir.isFile()) {
      json_filelist[filecount]["type"] = "file";
      json_filelist[filecount]["size"] = dir.fileSize();
      json_filelist[filecount]["lastmod"] = String(filecreation.tm_mday) +"/" + String(filecreation.tm_mon+1) + "/" + String(filecreation.tm_year+1900);
    }
    if (dir.isDirectory()) {
      json_filelist[filecount]["type"] = "dir";
      json_filelist[filecount]["size"] = 0;
      json_filelist[filecount]["lastmod"] = "";
    }
    filecount++;
  }
  //serializeJson(json_filelist, Serial);
  //Serial.println();
  serializeJson(json_filelist, responsestring);
  return String(responsestring);
}

//****************************************************************************
