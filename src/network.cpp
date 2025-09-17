/* Network related functions 
*
*   Arjen Vellekoop
*   September 2025
*   Version 1.0
*
*
*/

#include "main.h"

// Initialize WiFi
bool initWiFi(void) {

  time_t  WiFi_timeout;
  IPAddress _ip,_gw,_sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);
    
  WiFi.mode(WIFI_STA);

  if (!WiFi.config(_ip, _gw, _sn)) {
    Serial.println("STA Failed to configure");
    return false;
  }
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  WiFi_timeout = millis();

  while(WiFi.status() != WL_CONNECTED) {
    if (millis() - WiFi_timeout >= 10000UL) { // 10 secs
      Serial.println("Failed to connect.");
      return false;
    }
    Serial.print(".");        
    delay(500);
  }

  Serial.println(WiFi.localIP());
  return true;
}


bool    SetupWifiManager() {

  SetupFinished = false;

  // Connect to Wi-Fi network with SSID and password
  Serial.printf("Setting AP (Access Point)\n");
  // NULL sets an open Access Point
  WiFi.softAP("YADE WiFiManager", NULL);

  IPAddress IP = WiFi.softAPIP();
  Serial.printf("AP IP address: ");
  Serial.println(IP); 

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });
    
  server.on("/network", HTTP_GET, [](AsyncWebServerRequest *request) {

    //List all parameters
    int params = request->params();
    for(int i=0;i<params;i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()) { //p->isPost() is also true
        Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      }
      else if(p->isPost()) {
        Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } 
      else {
        Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
        if (p->name() == "ssid") {
          strcpy(ssid, p->value().c_str());
        }
        if (p->name() == "pass") {
          strcpy(password, p->value().c_str());
        }
        if (p->name() == "ip") {
          strcpy(static_ip, p->value().c_str());
        }
        if (p->name() == "gateway") {
          strcpy(static_gw, p->value().c_str());
        }
        if (p->name() == "subnet") {
          strcpy(static_sn, p->value().c_str());
        }
      }
    }    
    Serial.printf("static_ip: %s\n", static_ip);
    Serial.printf("static_gw: %s\n", static_gw);
    Serial.printf("static_sn: %s\n", static_sn);
    Serial.printf("ssid: %s\n", ssid);
    Serial.printf("password: %s\n", password);    
    if (strlen(ssid) && strlen(password)) {
      SetupFinished = true;
    }
    request->send(200, "text/plain", "Done. ESP will restart");
  });
  while (!SetupFinished) {
    delay(500);
    yield();
  }

  //save the custom parameters to FS
  Serial.println("saving config.json");
  JsonDocument json;
  json["pass"] = password;
  json["ssid"] = ssid;
  json["ip"] = static_ip;
  json["gateway"] = static_gw;
  json["subnet"] = static_sn;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  serializeJson(json, Serial);
  serializeJson(json, configFile);
  configFile.close();
  //end save

  delay(3000);
  ESP.restart();
  
  return(true);
}