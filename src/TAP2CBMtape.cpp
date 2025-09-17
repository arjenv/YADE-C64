/***************************************************
 * 
 * TAP2CBMtape
 * converts and outputs a .TAP to C64 tape (serial)
 *  as if it was a datasette
 * 
 * 
 * Arjen Vellekoop
 * 10 september 2025
 * 
 *******************************************************/




#include "main.h"

void TAP2CBMtape(File C64PRG, uint8_t TapeState, bool panicbutton) {

    // for now (File) C64PRG is global.

    uint8_t     i;
    uint16_t    cbcountdeep = BUFFER_SIZE; // for debugging ringbuffer. How deep does it go? 
    uint32_t    nrofbytes = 0;
    uint32_t    longsilence;
    bool        former_motorstate, current_motorstate = true;
    bool        timer1_running;

    for (i=0; i<0x14; i++) C64PRG.read();  //skip 1st 20 bytes

    timer1_running = false;

    while (C64PRG.available() && (TapeState == STATE_HEADER || TapeState == STATE_PROGRAM)) { 
      if (cb.count < BUFFER_SIZE-1) { //push next byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
        PRGbyte = C64PRG.read();
        if (cbcountdeep > cb.count) { // debugging ringbuffer
          cbcountdeep = cb.count;
        }
        if (PRGbyte == 0x00) { //special case: insert a (long) silence. Next 3 bytes define the Time:
          Serial.printf("\n\nPRGByte = 0x00, nrofbytes: %i\n", nrofbytes);
          longsilence = C64PRG.read() | C64PRG.read()<<8 | C64PRG.read()<<16;
          Serial.printf("longsilence: %i\n\n", longsilence);

          cb.count++;                                 // push it 
          cb.buffer[cb.head] = longsilence/4;        // divide by 4 to get the actual time
          cb.head = (cb.head + 1) % BUFFER_SIZE;
          cb.count++;
          cb.buffer[cb.head] = 0x30; // trick: need to finish the total period
          cb.head = (cb.head + 1) % BUFFER_SIZE;  
                  
        }
        else {
          push_ringbuffer_twice(&cb, PRGbyte);
        }

        nrofbytes++;
        if (!(nrofbytes%100)) {
          Serial.printf("nrofbytes: %i --> %i\r", C64PRG.size(), nrofbytes); 
        }
      }
      else { // ringbuffer is filled, start the timer
        if (!timer1_running) {
          timer1_write(5000); // start the timer
          cbcountdeep = BUFFER_SIZE;
          timer1_running = 1;
        }
      }
      
      former_motorstate = current_motorstate;
      current_motorstate = digitalRead(TAPEMOTOR);

      if (current_motorstate - former_motorstate == 1) { // motor is toggled on again
        TapeState++; // From STATE_HEADER to STATE_PROGRAM part. This happens more then once? Then FAULT
        Serial.printf("\nMotor switched on\n");
        Serial.printf("Starting timer again\n");
        Serial.printf("TapeSense: %i\n", digitalRead(TAPESENSE));
        Serial.printf("cb.count: %i\n", cb.count);
        Serial.printf("nrofbytes: %i\n", nrofbytes);
        Serial.printf("TapeState: %i\n", TapeState);
        Serial.printf("cbcountdeep: %i\n", cbcountdeep);
        Serial.printf("current_motorstate: %i\n", current_motorstate);
        Serial.printf("former_motorstate: %i\n", former_motorstate);
        timer1_write(5000); //restart timer interrupt
      }
      if (current_motorstate - former_motorstate == -1) { // motor is toggled off 
                                                    // Only when TapeState = TAPE_HEADER. 
                                                    // Else FAULT 
                                                    // Or there were some extra bytes
                                                    // but the program already loaded so C64
                                                    // has switched off motor before all bytes were sent
        Serial.printf("\nMotor switched off\n");
        Serial.printf("TapeSense: %i\n", digitalRead(TAPESENSE));
        Serial.printf("cb.count: %i\n", cb.count);
        Serial.printf("nrofbytes: %i\n", nrofbytes);
        Serial.printf("TapeState: %i\n", TapeState);
        Serial.printf("cbcountdeep: %i\n", cbcountdeep);
        Serial.printf("current_motorstate: %i\n", current_motorstate);
        Serial.printf("former_motorstate: %i\n", former_motorstate);
        if (TapeState != STATE_HEADER) {
          Serial.printf("Motor shut off prematurely\n");
          TapeState = STATE_FAULT;
        }
      }

      if (!cb.count) { // check ringbuffer. If 0 items then FAULT or finished
        TapeState = STATE_FAULT;
        Serial.printf("Timer has stopped prematurely\n\n");
        Serial.printf("TapeSense: %i\n", digitalRead(TAPESENSE));
        Serial.printf("cb.count: %i\n", cb.count);
        Serial.printf("nrofbytes: %i\n", nrofbytes);
        Serial.printf("TapeState: %i\n", TapeState);
        Serial.printf("cbcountdeep: %i\n", cbcountdeep);
        Serial.printf("current_motorstate: %i\n", current_motorstate);
        Serial.printf("former_motorstate: %i\n", former_motorstate);

      }
      if (panicbutton) { // added a panic button to get out of the while loop when it hangs
        TapeState = STATE_FAULT;
        Serial.printf("Pressed PANIC button\n\n");
      }
      if (TapeState >= STATE_FAULT) {
        Serial.printf("FAULT State Flushing.... %i\n", TapeState); // TapeState should never be over STATE_FAULT = 4
        Serial.printf("TapeSense: %i\n", digitalRead(TAPESENSE));
        Serial.printf("cb.count: %i\n", cb.count);
        Serial.printf("nrofbytes: %i\n", nrofbytes);
        Serial.printf("cbcountdeep: %i\n", cbcountdeep);
        Serial.printf("current_motorstate: %i\n", current_motorstate);
        Serial.printf("former_motorstate: %i\n", former_motorstate);

      }
      yield(); // avoid watchdog in this while loop.
    }
    Serial.printf("\nRead %i bytes\n\n", nrofbytes);
    Serial.printf("TapeMotor = %i\n", current_motorstate);
    // wait for buffer to empty


    timeout = millis(); // while loop. Always safe to add timeout
    timeoutflag = false;
    while(cb.count && digitalRead(TAPEMOTOR) && !timeoutflag) {
      Serial.printf("cb_count: %i             \r", cb.count);
      if ((millis() - timeout) > 1000) {
        Serial.printf("\nStuck in while loop..   Aborting\n\n");
        timeoutflag = true;
      }
    }
    Serial.printf("End TAP2CBMtape. cbcountdeep: %i\n", cbcountdeep);
    
  } 

