/* Handle Telnet requests
*
*   Arjen Vellekoop
*
*   May 22, 2024
*
*         R = Reset
*         C = Clear and stop (stop telnet)
*         F =  show FreeHeap
*         V = Version
*/

//#include <Arduino.h>
//#include <TelnetStream.h>       // Version 1.2.5 latest sept 17, 2023 //  https://github.com/jandrassy/TelnetStream
#include "main.h"

void   HandleTelnet(void) {

  switch (toupper(TelnetStream.read())) { // commands recognised by Telnet
    case 'R':
    TelnetStream.stop();
    delay(100);
    ESP.reset();
      break;
    case 'C':
      TelnetStream.println("bye bye");
      TelnetStream.flush();
      TelnetStream.stop();
      break;
    case 'F':
      TelnetStream.printf("Freeheap: %d\n\r", ESP.getFreeHeap());
      break;
    case 'V':
      TelnetStream.printf("Version: %s\n\r", Version);
      break;
    case 'L':
      TelnetStream.printf("Tape sense: %i\n\r", digitalRead(TAPESENSE));
      TelnetStream.printf("Tape motor: %i\n\r", digitalRead(TAPEMOTOR));
      TelnetStream.printf("Tape read: %i\n\r", digitalRead(TAPEREAD));
      TelnetStream.printf("Tape write: %i\n\r", digitalRead(TAPEWRITE));
      break;
    case 'S': // toggle tapesense
      digitalWrite(TAPESENSE, !digitalRead(TAPESENSE));
    break;
    case 'B': // toggle taperead output
      digitalWrite(TAPEREAD, !digitalRead(TAPEREAD));
    break;
    case 'W': 
      TelnetStream.printf("RSSI: %i\n", WiFi.RSSI());
      break;
    case 'H':
      TelnetStream.println("R: Reset");
      TelnetStream.println("C: Clear and stop");
      TelnetStream.println("F: FreeHeap");
      TelnetStream.println("V: Version");
      TelnetStream.println("W: Wifi signal Strength");
      TelnetStream.println("B: Toggle TapeRead output");
      TelnetStream.println("S: Toggle TapeSense output");
      TelnetStream.println("L: List latest info");
      break;
    default:
      while(TelnetStream.available())
        TelnetStream.read(); // empty buffer. Required because at startup the buffer is filled with garbage
                            // and after a single letter command there is still a return and Newline.
  } //end switch
}
