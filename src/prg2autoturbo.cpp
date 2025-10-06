/**************************************************
 * 
 * prg2turbo_auto(File C64PRG, char *header)
 * 
 * Requires an (open) .PRG file and a filles header array.
 * 
 * First loads a small program the C64 can read. 
 * This program then invokes a fastload routine to get the actual Program
 * 
 * Goto 'settings' in the webpage to change some variables to speed up this pocess.
 * 
 * Arjen Vellekoop
 * 16 september 2025
 * 
 * 
 *************************************************************/


#include "main.h"
#define	PILOTBYTE	0x02

void byte2turbo_auto(uint8_t * bitstring, uint8_t byte, uint8_t TurboThreshold) {
	// [MSB|Bit1|Bit2|Bit3|Bit4|Bit5|Bit6|LSB]
	int i;
	for (i=0; i<8; i++) {
		if (byte & (1<<i)) { // test bit
			bitstring[7-i] = int(TurboThreshold/8.0 + 0.5) + 4;
			//printf("1 ");
		}
		else {
			bitstring[7-i] = int(TurboThreshold/8.0 + 0.5) - 4;
			//printf("0 ");
		}
	}
}

void	check_timer1running(bool *timer1_running, uint16_t *cbcountmin, int row) {
				if (!*timer1_running) {
					timer1_write(5000); // start the timer
    	      		*timer1_running = true;
					*cbcountmin = BUFFER_SIZE;
					if (PO) tee.printf("Started timer1 (5000) in row %i\n", row);
					if (PO) tee.printf("cbcount: %i\n\n", cb.count);
				}
			}


void prg2turbo_auto(File C64PRG, char *header) {


	/**************************************************
	*
	*	Maggotmania total duration (incl waiting for motor AND 10secs delay between normal/turboload) without changing variables: 64435 msec
	*		prgduration: 24685 msec
	*	CBMsyncpulse = 700 (600 does not work) --> 51823 and 24685 msec
	*	No delay between normal/turbo --> 42270 and 24697
	*	TurboThreshold from 254 to 120 (112 works but 104 does not) --> 31720 and 12476
	*		Since TurboThreshold is divided by 8 make it a multiple of 8.
	*	Skip 2nd Pilotbyte (503) --> 28196 and 11887
	*	TurbosyncLeader from 1271 to 10 --> 29395 and 10844
	*	TurboPilotebyte from 503 to 10 --> 29449 and 10440
	*	skip trailer pulses --> 26014 and 8501
	*	Hit space in between loading as to carry on immediately --> 16261 and 8501
	*
	*
	*
	********************************************************/

	tee.printf("prg2turbo_auto..... \n\n");

	// debugging the output:
	#define TAPOUTPUT 0 // send to rs232

	char		basename[30];
	uint8_t 	startaddressLowbyte, startaddressHighbyte, endaddressLowbyte, 
					endaddressHighbyte, prgchecksum, bitsequence[22];
	uint16_t 	i, j, startaddress, endaddress;
	uint32_t	tapsize;
	File		YADE_FL, YADE_header;
	uint16_t	cbcountmin = BUFFER_SIZE;
	time_t		prgduration, totalprgduration;
	bool		timer1_running;

	#if TAPOUTPUT
	uint8_t tapheader[] = {0x43, 0x36, 0x34, 0x2d, 0x54, 0x41, 0x50, 0x45, 0x2d, 0x52, 0x41, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t			silence[] = {0x00, 0x98, 0x01, 0x05}; // LowHigh format --> 328088 pulses
	#endif

	// Start with a CBM normal speed load of YADE_FL.prg with YADE_header.bin in the header after the filename
	totalprgduration = millis(); // let's measure load duration..

	// Opening YADE_FL.prg for startaddress, endaddres etc

	YADE_FL = LittleFS.open("/YADE_FL_set2B2.prg", "r");
    if (!YADE_FL) {
      tee.printf("Cannot open /YADE_FL.prg\n");
      // need some error code here to skip rest
	}
    else {
        tee.printf("Opened YADE_FL.prg\n");
        YADE_FL.read();
        YADE_FL.read(); // skip 1st two bytes as they represent startaddress.
                        // but that is already in the header[] 
    }
    prg2tap(YADE_FL, header);

	// cleaning up
    if (PO) tee.printf("Closing file %s\n", "YADE_FL.prg");
    YADE_FL.close();
    interrupt_occured = 0;
    if (PO) tee.printf("Resetting ringbuffer\n");
    reset_RingBuffer(&cb);
	if (PO) tee.printf("Pressing stop on taperecorder...\n");
	digitalWrite(TAPESENSE, 1);
	  
    ReadOutput = 1; // need to reset this!

	tee.printf("prg2turbo_auto  In the actual game..... \n\n");

	#if TAPOUTPUT
	uint8_t tapheader[] = {0x43, 0x36, 0x34, 0x2d, 0x54, 0x41, 0x50, 0x45, 0x2d, 0x52, 0x41, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t			silence[] = {0x00, 0x98, 0x01, 0x05}; // LowHigh format --> 328088 pulses
	#endif

		
	// Now again for turbo speed
	//
	// Structure:
	// 	Each turbo file is made of a HEADER section and a DATA one, without gaps between them. 
	// Turbo blocks:
	// 
	// 	Threshold: 0x0107 (263) clock cycles (TAP value: 0x21)
	// 	Bit 0 pulse: 0x1A
	// 	Bit 1 pulse: 0x28
	// 	Endianess: MSbF
	//
	// 	Pilot byte: 0x02 (size: the genuine tape mastering program saves 256*4+247 of them for HEADER and 256+247 for DATA)
	// 	Sync train: 0x09, 0x08, ... 0x01
	//
	// 	01 byte: File type
	//
	// 		0x00 for DATA
	// 		0x01 (or any odd number) for HEADER (relocatable program)
	// 		0x02 (or any even number) for HEADER (non-relocatable program)
	//
	// 	Header file
	// 	-----------
	// 		02 bytes: Load address  (LSBF)
	// 		02 bytes: End address+1 (LSBF)
	// 		01 byte : Whatever $00B0 (Tape Timing) contained at the time of saving
	// 		16 bytes: Filename, padded with 0x20
	// 		171 bytes: Payload, filled with 0x20 
	//
	// 	Data file
	// 	---------
	// 		n bytes : Data
	// 		01 byte : XOR Checksum of Data
	// 
	// 	Trailer: After the DATA file a trailer follows, made up of bytes whose value is $00. The genuine Tape Mastering program saves 255 of them. 
	//
	//		Total bytes (for TAP Header):
	//		1271*8(sync) + 9*8 (countdown) + 1*8 (filetype) + 192*8 (header) + 503*8 (syncdata) + 9*8 (countdown) + 1*8 (filetype) + {fsize-2}*8 (DATA) + 1*8 (Checksum) + 255*8 (trailer 0x00)
	//		17936 + {fsize-2}*8
	//
	
	prgduration = millis(); // measure .PRG duration

	// Need to rewind the .PRG file. We need startaddress again
	C64PRG.seek(0, SeekSet);

	tapsize = 17936 + 8*(C64PRG.size()-2);
	#if TAPOUTPUT 
		tapheader[16] = (tapsize&0xFF);
		tapheader[17] = (tapsize&0xFF00)>>8;
		tapheader[18] = (tapsize&0xFF0000)>>16;
		tapheader[19] = (tapsize&0xFF000000)>>24;
	#endif
	if (PO) printf ("turbo TAPsize:\t%i\n", tapsize);

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
	// can be much shorter cause the autoload only needs the header[0-4]
	
	header[0] = 0x02; // file type non relocatable prg
	header[1] = startaddressLowbyte; // startaddress low byte
	header[2] = startaddressHighbyte; // startaddres high byte
	header[3] = endaddressLowbyte; // endaddress low byte
	header[4] = endaddressHighbyte; // endaddress High byte


	// rest of the header not needed...

	timeoutflag = 0;
	timeout = millis();
	timer1_running = false;
	byte2turbo_auto(bitsequence, PILOTBYTE, TurboThreshold);
	//delay(1000); // increase value if you want to see the message on the C64 "Press play on tape"

	digitalWrite(TAPESENSE, 0); // press play
	tee.printf("Pressed play again.....\n\n");

	// experiment with the pilotbyte it is far too long now..
	// default = 1271
	// variable = TurbosyncLeader

//	for (i=0; i<1271; i++) { // pilot 256*4+247 = 1271 --> ('02' --> 7x 0x1A + 28)  =  222*2*4*1271 us = 2.25 sec
	for (i=0; i<TurbosyncLeader; i++) { 
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 8000) {
				tee.printf("\nprg2turbo_auto Timeout pilot (default:1271).. %i\n", TurbosyncLeader);
				timeoutflag = 1;
				tee.printf("cbcount: %i i=%i\n", cb.count, i);
			}
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				if (cb.count < cbcountmin) cbcountmin = cb.count;
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
			}
			else {
				check_timer1running(&timer1_running, &cbcountmin, 1);
			}
			yield();
		}
	}
	if (PO) Serial.printf("End Pilot (default 1271) %i\n", TurbosyncLeader);
	if (PO) Serial.printf("cbcountmin %i\n\n", cbcountmin);

	timeout = millis();
	for (i=0x09; i>0x00; i--) { // 1st countdown seq
		byte2turbo_auto(bitsequence, i, TurboThreshold);
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 200) {
				Serial.printf("\nprg2turbo_auto 1st countdown sequence timeout..");
				timeoutflag = 1;
				Serial.printf("cbcount: %i\n", cb.count);
			}
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
			}
			else {
				check_timer1running(&timer1_running, &cbcountmin, 2);
			}
			yield();
		}
	}
	if (PO) Serial.printf("End 1st countdown sequence\n");

	timeout=millis();
	cbcountmin = BUFFER_SIZE;

	// reduce i to 5 here, the rest is skipped anyway
	for (i=0; i<5; i++) { // Header
		byte2turbo_auto(bitsequence, header[i], TurboThreshold);
		j=0;
		while (!timeoutflag && j<8) {
			if (cb.count<cbcountmin) cbcountmin = cb.count;
			if ((millis() - timeout) > 1000) {
				Serial.printf("\nprg2turbo_auto Header timeout..");
				timeoutflag = 1;
				Serial.printf("cbcount: %i\n", cb.count);
			}
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
			}
			else {
				check_timer1running(&timer1_running, &cbcountmin, 3);
			}
			yield();
		}
	}
	if (PO) Serial.printf("End header \n");
	if (PO) Serial.printf("CBcount: %i\n", cb.count);
	if (PO) Serial.printf("cbcountmin %i\n\n", cbcountmin);


	timeout=millis();
	// experiment with the value for the next pilotbyte
	// variable TurboPilotbyte default 503

	for (i=0; i<TurboPilotebyte; i++) { // pilot 503
		byte2turbo_auto(bitsequence, PILOTBYTE, TurboThreshold);
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 5000) {
				Serial.printf("\nprg2turbo_auto Pilot 503 timeout..");
				timeoutflag = 1;
				Serial.printf("cbcount: %i j=%i i=%i\n", cb.count, j, i);
			}
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
			}
			else {
				check_timer1running(&timer1_running, &cbcountmin, 4);
			}
			yield();
		}
	}
	if (PO) Serial.printf("End Pilot (default 503) %i\n", TurboPilotebyte);

	// no motor stop in auto mode
	// this is redundant. delete the 2nd pilotbyte

	timeout=millis();
	for (i=0x09; i>0x00; i--) { // 2nd countdown seq
		byte2turbo_auto(bitsequence, i, TurboThreshold);
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 200) {
				Serial.printf("\nprg2turbo_auto 2nd countdown sequence..");
				timeoutflag = 1;
				Serial.printf("cbcount: %i\n", cb.count);
			}
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
			}
			else {
				check_timer1running(&timer1_running, &cbcountmin, 5);
			}
			yield();
		}
	}
	if (PO) Serial.printf("End 2nd countdown sequence\n");

	timeout = millis();
	byte2turbo_auto(bitsequence, 0, TurboThreshold); // filetype 0=DATA
	j=0;
	while (!timeoutflag && j<8) {
		if ((millis() - timeout) > 200) {
			Serial.printf("\nprg2turbo_auto timeout filetype=0..");
			timeoutflag = 1;
			Serial.printf("cbcount: %i\n", cb.count);
		}
		if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
			push_ringbuffer_twice(&cb, bitsequence[j]);
			#if TAPOUTPUT
				Serial.write(bitsequence[j]);
			#endif
			j++;
		}
			else {
				check_timer1running(&timer1_running, &cbcountmin, 6);
			}
		yield();
	}
	if (PO) Serial.printf("End filetype 0=DATA\n");

	prgchecksum = 0;
	timeout = millis();
	cbcountmin = BUFFER_SIZE;
	for (i=0; i<C64PRG.size()-2; i++) { // the actual DATA
		PRGbyte = C64PRG.read();
		prgchecksum^=PRGbyte; //need to do this only one round but this is easier
		byte2turbo_auto(bitsequence, PRGbyte, TurboThreshold);
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 200) {
				Serial.printf("\nprg2turbo_auto DATA timeout..");
				timeoutflag = 1;
				Serial.printf("cbcount: %i\n", cb.count);
			}
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				if (cb.count < cbcountmin) cbcountmin = cb.count;
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
				timeout=millis(); // need to reset cause filesize is unknown
			}
			else {
				check_timer1running(&timer1_running, &cbcountmin, 7);
			}
			yield();
		}
		if (!(i%100)) {
			if (PO) Serial.printf("PRGbyte: %i --> %i byte: %02x, CBcount: %i\n", C64PRG.size()-2, i, PRGbyte, cb.count);
		}
	}
	if (PO) Serial.printf("End DATA\n");
	if (PO) Serial.printf("cbcountmin %i\n\n", cbcountmin);

	timeout = millis();
	byte2turbo_auto(bitsequence, prgchecksum, TurboThreshold); // checksum
	j=0;
	while (!timeoutflag && j<8) {
		if ((millis() - timeout) > 200) {
			Serial.printf("\nprg2turbo_auto checksum..");
			timeoutflag = 1;
			Serial.printf("cbcount: %i\n", cb.count);
		}
		if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
			push_ringbuffer_twice(&cb, bitsequence[j]);
			#if TAPOUTPUT
				Serial.write(bitsequence[j]);
			#endif
			j++;
		}
		yield();
	}
	if (PO) Serial.printf("End Checksum\n");

	// Trailerbyte is redundant in this turbomode

    //if (PO) Serial.printf("Closing file %s\n", C64filename);
    //C64PRG.close();

	// wait for all pulses..
	timeout = millis();
//	while (!timeoutflag && cb.count && digitalRead(TAPEMOTOR)) { //sync sequence short
	while (!timeoutflag && digitalRead(TAPEMOTOR)) { //sync sequence short
		if ((millis()-timeout) >2000) {
			Serial.printf("\nprg2turbo_auto Timeout closing pulses..");
			timeoutflag = 1;
		}
		if (PO) Serial.printf("Sending pulses from buffer until motor stops.... %i\n", cb.count);
	}

	if (PO) tee.printf("Program Duration: %lli\n\n", millis()-prgduration);
	if (PO) tee.printf("Total Duration: %lli\n\n", millis()-totalprgduration);
	
}
