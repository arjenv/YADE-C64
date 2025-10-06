/********************************************
 * 
 * SaveTAP
 * 
 * Arjen Vellekoop
 * September 2025
 * 
 * Saves TAP file
 * 
 * Requires:    cleanupTAP
 *              RecOverwrite
 *              panicbutton
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * decimal 43 start&$FF  Low byte
 * 44 start      High byte
 * 45  End       Low byte
 * 46  End       High Byte
 * 
 * 
 * 
 * 
 * 
 * 
 *************************************************/

 #include    "main.h"

int SaveTAP(bool cleanupTAP, bool panicbutton, bool RecOverwrite) {

  uint16_t          cbcountmax;
  uint8_t           tapheader[] = {0x43, 0x36, 0x34, 0x2d, 0x54, 0x41, 0x50, 0x45, 0x2d, 0x52, 0x41, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  FSInfo            FS_Save;
  uint32_t          nrofbytes;
  File              C64TAPFile;
  uint8_t           waitformotor;
  char              TAPbyte;
  bool              CurrentTAP = 0; // 0=normal speed, 1=Turbospeed
  uint32_t          C64TAP_size;
  uint8_t           TAPcbm, TAPTurbo;
  bool              motorstopped = false;
  uint32_t          distribution[256];

  for (nrofbytes=0; nrofbytes<256; nrofbytes++) {
    distribution[nrofbytes]=0;
  }
  LittleFS.info(FS_Save);
  cbcountmax = 0;
  nrofbytes = 0;
  Serial.printf("Totalbytes: %i\n", FS_Save.totalBytes);
  Serial.printf("Usedbytes: %i\n", FS_Save.usedBytes);
  if (cleanupTAP) Serial.printf("Will try to clean up TAP\n");
  else Serial.printf("Raw TAP, no cleanup\n");
  if (RecOverwrite) Serial.printf("Overwriting %s\n", C64filename);
  else Serial.printf("Creating new file: %s\n", C64filename);
    
  if ((FS_Save.totalBytes-FS_Save.usedBytes) < 100000) {
    Serial.printf("Less than 100k space. Delete some files before trying this... %i\n", FS_Save.totalBytes-FS_Save.usedBytes);
    return(1);
  }
  if (!(C64TAPFile = LittleFS.open(C64filename, "w"))) {
    tee.printf("Cannot open this file %s\n", C64filename);
    return(2);
  }
  else {
    tee.printf("opened C64TAPFile File %s\n", C64filename);
  }
  for (int i=0; i<20; i++) {
    C64TAPFile.printf("%c", tapheader[i]);
  }
  timer1_detachInterrupt();
  reset_RingBuffer(&cb);

  // wait for the motor
  digitalWrite(TAPESENSE, 0); // press play
  waitformotor=0;

  attachInterrupt(digitalPinToInterrupt(D6), ISRSave, RISING);
  Serial.printf("Program name: %s\n", C64filename);

  while (!digitalRead(TAPEMOTOR) && (waitformotor<20)) { // wait for the motor voltage
    Serial.printf("SAVE: Wait for the motor.. %i\n", waitformotor);
    waitformotor++;      
  }
  Serial.printf("Motor started cbcount: %i\n", cb.count);
  if (waitformotor>19) { //error
    Serial.printf("Error: Motor did not start in time\n");
    C64TAPFile.close();
    LittleFS.remove(C64filename);
    return(3);
  }
  else {
    // Determine whether output is normal speed TAP or TurboTAP
    // Normal speed: expect around 0x2F, 0x42, 0x56
    // Turbo speed: expect around 0x1A, 0x28
    // TAP starts with a Pilotbyte. Either 0x2F (=normal speed) or (mostly) 0x1A (=Turbospeed)

    //Serial.printf("Will try to clean up TAP\n");
    timeout = millis();
    timeoutflag = false;
    while(cb.count<20 && !panicbutton && !timeoutflag) {
      Serial.printf("Processing TAP format...\r");
      if (((millis() - timeout)) > 2000) {
        Serial.printf("\nTimeout in Processing TAP..\n");
        timeoutflag = true;
        return(4);
      }
    }
    if (!timeoutflag) {
      TAPcbm = 0;
      TAPTurbo = 0;
      for (nrofbytes=0; nrofbytes<20; nrofbytes++) { // check the first 20 bytes
        TAPbyte = cb.buffer[cb.tail+nrofbytes];
        if (TAPbyte<(0x1A+6)) TAPTurbo++;
        else if (TAPbyte<(0x2F+6)) TAPcbm++;
      }        
      Serial.printf("\nTAP 0x1A: %i, 0x2F: %i\n", TAPTurbo, TAPcbm);
      if (TAPTurbo>TAPcbm) { // 0=normal speed, 1=Turbospeed
        CurrentTAP = 1;   
        Serial.printf("Recognised Turbo speed TAP\n");
      }
      else {
        CurrentTAP = 0;   
        Serial.printf("Recognised Normal speed TAP\n");
      }
      nrofbytes = 0;
    }
      
  }

  // Cannot rely on the motor voltage alone. It seems it shuts down before all bytes are sent
  // so we introduce a timeout after the motorvoltage is low
  // in normal speed the motorvoltage will go (shortly) low after sending the 2xHeader.

  timeoutflag = false;
  while(!timeoutflag) {
    while (cb.count) {
      if (cb.count>cbcountmax) cbcountmax = cb.count;
      PRGbyte = pull_ringbuffer(&cb);
      TAPbyte = PRGbyte;

      // cleanupTAP
      if (cleanupTAP) {
        if (!CurrentTAP) { // normal speed TAP
          if ((PRGbyte > (CBMPULSE_SHORT-10)) && (PRGbyte < CBMPULSE_LONG+10)) {         // can be 0X2F, 0X42, 0X56
            TAPbyte = CBMPULSE_SHORT;
            if (PRGbyte > (CBMPULSE_SHORT+10)) {       // 0X42 or 0X56
              TAPbyte = CBMPULSE_MEDIUM;
              if (PRGbyte > (CBMPULSE_MEDIUM+10)) {     // 0X56
                TAPbyte = CBMPULSE_LONG;
              }
            }
          }
        }
        else { // Turbo Speed TAP
          if ((PRGbyte > (TURBO_SHORT-7)) && (PRGbyte < TURBO_LONG+7)) {         // Either 0x1A or 0x28
            TAPbyte = TURBO_SHORT;
            if (PRGbyte > (TURBO_SHORT+7)) {       // 
              TAPbyte = TURBO_LONG;
            }
          }
        }
      }
      // end cleanupTAP

      if (!nrofbytes) { // 1st byte is wrong so reset
        if (CurrentTAP) TAPbyte = TURBO_SHORT;
        else TAPbyte = CBMPULSE_SHORT;
      }

      if (TAPbyte > CBMPULSE_LONG+9) {  // special case, this is a pause
                                        // {0x00, 0x98, 0x01, 0x05}; // LowHigh format --> 328088 pulses

        Serial.printf("Inserting 300msec Pause\n");
        C64TAPFile.write(0x00);
        C64TAPFile.write(0x98);
        C64TAPFile.write(0x01);
        TAPbyte = 0x05; // this byte will be saved next
        nrofbytes+=3;
      }

      distribution[(uint8_t) TAPbyte]++; // how many?
      C64TAPFile.write(TAPbyte);
      nrofbytes++;
      if (!(nrofbytes%1000)) {
        Serial.printf("%i %i %i\n", nrofbytes, cb.count, cbcountmax);
      }
  
      yield();
    }

    if (!digitalRead(TAPEMOTOR)) { // motor has halted, start timeout
      Serial.printf("Motor voltage OFF: %i %i\n", cb.count, nrofbytes);
      if (!motorstopped) timeout = millis();
      motorstopped = true;
      if ((millis() - timeout) > 100) {
        Serial.printf("Reached Timeout after motor stopped\n");
        timeoutflag = true;
      }
    }
    else {
      motorstopped = false;
    }

    yield();
  }

  if (PO) tee.printf("Closing file %s\n", C64filename);
  C64TAPFile.close();
  detachInterrupt(digitalPinToInterrupt(D6));
  if (PO) Serial.printf("nrofbytes: %i\n", nrofbytes);
  if (PO) tee.printf("cbountmax: %i\n", cbcountmax);
  if (PO) tee.printf("Resetting ringbuffer\n");
  reset_RingBuffer(&cb);
  if (PO) tee.printf("Stopping taperecorder...\n");
  digitalWrite(TAPESENSE, 1);
  timer1_attachInterrupt(timer1ISR);

  //postprocessing
  C64TAPFile = LittleFS.open(C64filename, "r+");
  C64TAP_size = C64TAPFile.size() - 20; // TAPfile header size is the Filesize minus headersize (20)
	if (PO) tee.printf("TAPsize:\t%i\n", C64TAPFile.size());
  C64TAPFile.seek(16, SeekSet);

  C64TAPFile.write(C64TAP_size&0xFF);
  C64TAPFile.write((C64TAP_size&0xFF00)>>8);
	C64TAPFile.write((C64TAP_size&0xFF0000)>>16);
	C64TAPFile.write((C64TAP_size&0xFF000000)>>24);
  //if (!CurrentTAP) C64TAPFile.write(CBMPULSE_SHORT); // replace 1st byte since it is wrong
  //else C64TAPFile.write(TURBO_SHORT);

  if (PO) {tee.printf("After setting size\n");}

  for (nrofbytes=0; nrofbytes<256; nrofbytes++) {
    if (PO) {
      if (distribution[nrofbytes]) Serial.printf("distribution: %02X\t\t%i\n", nrofbytes, distribution[nrofbytes]);
    }
  }


  C64TAPFile.close();
  return(0);
}
