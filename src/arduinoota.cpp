/* Init Arduino OTA requests
*
*   Arjen Vellekoop
*
*   30 June 2025
*
*   Initialisation of OTA
*         
*/

#include "main.h"

void   InitArduinoOTA(void) {

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    char type[11];
    if (ArduinoOTA.getCommand() == U_FLASH) {
      strcpy(type, "sketch");
    } else {  // U_FS
      strcpy(type, "filesystem");
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    tee.printf("Start updating %s\n", type);
  });
  ArduinoOTA.onEnd([]() {
    tee.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    tee.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    tee.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      tee.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      tee.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      tee.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      tee.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      tee.println("End Failed");
    }
  });
  ArduinoOTA.begin();

}
