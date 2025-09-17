/*********************************************************
 * 
 * void prg2turbo(File C64PRG, char* header)
 * 
 * 	C64PRG: .PRG file 
 * 	Header[] assumed to be filled
 * 
 * Converts a .PRG file to C64 Tape format
 * 
 * Arjen Vellekoop
 * 16 september 2025
 * 
 * 
 * .PRG Format:
 *
 * 2 bytes indicating the start address (LowHigh)
 * rest of the bytes are actual program bytes.		
 * 
 **********************************************************/

#include "main.h"

#define	TURBO_SHORT	0x1A
#define	TURBO_LONG	0x28
#define TURBO_SHORT_AUTO 0xd
#define TURBO_LONG_AUTO 0x14

#define	PILOTBYTE	0x02
#define	TRAILERBYTE	0x00

void byte2turbo(uint8_t * bitstring, uint8_t byte) {
	// [MSB|Bit1|Bit2|Bit3|Bit4|Bit5|Bit6|LSB]
	int i;
	for (i=0; i<8; i++) {
		if (byte & (1<<i)) { // test bit
			bitstring[7-i] = TURBO_LONG;
			//printf("1 ");
		}
		else {
			bitstring[7-i] = TURBO_SHORT;
			//printf("0 ");
		}
	}
}

void prg2turbo(File C64PRG, char* header) {

	uint32_t	tapsize;
	uint8_t		prgchecksum, bitsequence[22];
	uint16_t 	i, j;
	uint16_t	cbcountmin = BUFFER_SIZE;
	bool		timer1_running;

	#if TAPOUTPUT
	uint8_t tapheader[] = {0x43, 0x36, 0x34, 0x2d, 0x54, 0x41, 0x50, 0x45, 0x2d, 0x52, 0x41, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t			silence[] = {0x00, 0x98, 0x01, 0x05}; // LowHigh format --> 328088 pulses
	#endif

		
	// Turbo speed
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
	
/*	WiFi.disconnect();
	Serial.printf("WiFi Disconnecting: ");
  	while (WiFi.status() == WL_CONNECTED) {
    	delay(500);
    	Serial.print(".");
  	}
  	Serial.printf("\nWiFi disconnected\n");
*/

	tapsize = 17936 + 8*(C64PRG.size()-2);
	#if TAPOUTPUT 
		tapheader[16] = (tapsize&0xFF);
		tapheader[17] = (tapsize&0xFF00)>>8;
		tapheader[18] = (tapsize&0xFF0000)>>16;
		tapheader[19] = (tapsize&0xFF000000)>>24;
	#endif
	if (PO) tee.printf("turbo TAPsize:\t%i\n", tapsize);

	timer1_running = false;
	timeoutflag = 0;
	timeout = millis();
	byte2turbo(bitsequence, PILOTBYTE);
//	for (i=0; i<1271; i++) { // pilot 256*4+247 = 1271 --> ('02' --> 7x 0x1A + 28)  =  222*2*4*1271 us = 2.25 sec
	for (i=0; i<1271; i++) { 
		j=0;
		while (!timeoutflag && j<8) {
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				if (cb.count < cbcountmin) cbcountmin = cb.count;
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
			}
			else { // loaded the ringbuffer check for startup
		        if (!timer1_running) {
        		  	timer1_write(5000); // start the timer
          			timer1_running = 1;
					cbcountmin = BUFFER_SIZE;
				}
				if ((millis() - timeout) > 4000) {
					tee.printf("\nprg2turbo Timeout pilot 1271..");
					timeoutflag = 1;
					tee.printf("cbcount: %i i=%i\n", cb.count, i);
				}
				//Serial.printf("i, j, cb.count, cbcountmin: %i %i %i %i\n", i, j, cb.count, cbcountmin);
        	}
			yield();
		}
	}
	if (PO) Serial.printf("End Pilot 1271\n");
	if (PO) Serial.printf("cbcountmin %i\n\n", cbcountmin);

	timeout = millis();
	for (i=0x09; i>0x00; i--) { // 1st countdown seq
		byte2turbo(bitsequence, i);
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 200) {
				Serial.printf("\nprg2turbo 1st countdown sequence timeout..");
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
	}
	if (PO) Serial.printf("End 1st countdown sequence\n");

	timeout=millis();
	cbcountmin = BUFFER_SIZE;
	for (i=0; i<193; i++) { // Header
		byte2turbo(bitsequence, header[i]);
		j=0;
		while (!timeoutflag && j<8) {
			if(cb.count < cbcountmin) cbcountmin = cb.count;
			if ((millis() - timeout) > 1000) {
				Serial.printf("\nprg2turbo Header timeout..");
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
	}
	if (PO) Serial.printf("End header \n");
	if (PO) Serial.printf("CBcount: %i\n", cb.count);
	if (PO) Serial.printf("cbcountmin %i\n\n", cbcountmin);






	timeout=millis();
	cbcountmin = BUFFER_SIZE;
	for (i=0; i<503; i++) { // pilot 503
		byte2turbo(bitsequence, PILOTBYTE);
		j=0;
		while (!timeoutflag && j<8) {
			if(cb.count < cbcountmin) cbcountmin = cb.count;
			if ((millis() - timeout) > 5000) {
				Serial.printf("\nprg2turbo Pilot 503 timeout..");
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
			yield();
			if (!digitalRead(TAPEMOTOR)) { // motor stopped
				if (PO) Serial.printf("Motor off\n");
				Serial.printf("j=%i, i=%i cbcount=%i\n", j,i, cb.count);
				j=8;
				i=503;
			
			}
		}
	}
	if (PO) Serial.printf("End Pilot 503\n");
	if (PO) Serial.printf("cbcountmin %i\n\n", cbcountmin);






	// wait for the motor to stop THIS IS TRICKY!!
	if (PO) Serial.printf("Wait for the motor to stop:\n");
	i=0;
	timeout=millis();
	while (!timeoutflag && digitalRead(TAPEMOTOR)) {
		if ((millis()-timeout) > 3000) {
       		Serial.printf("\nprg2turbo Motor did not stop?   Aborting\n\n");
			timeoutflag = true;
		}
		if (PO) Serial.printf("Wait for the motor to stop: %i  %i\n", i++, cb.count);
	}

	timeout = millis();
	if (PO) Serial.printf("Waiting for motor voltage...\n");
	i=0;
	while(!timeoutflag && !digitalRead(TAPEMOTOR)) {
		if ((millis()-timeout) > 20000) { // should start within 20secs
       		Serial.printf("\nprg2turbo Motor did not start within 20secs..   Aborting\n\n");
			timeoutflag = true;
		}
		yield();
			
		if(!(i%1000)) {if (PO) Serial.printf("Waiting for motor voltage... %i\r", i);}
		i++;
	}
	if (PO) Serial.printf("\nEnd motor while loop...\n");

	#if TAPOUTPUT // not sure about this in the turbo mode
		for (i=0; i<4; i++) {
			Serial.write(silence[i]);
		}
	#endif
	if (!timeoutflag) { // start timer1 // check specs how long in turbo mode
		timer1_write(1640440); // 328msec we need a 'silence' here 328msec
		if (PO) Serial.printf("Timer1 started 300msec\n");
	}

	timeout=millis();
	cbcountmin = BUFFER_SIZE;
	for (i=0; i<503; i++) { // pilot 503
		byte2turbo(bitsequence, PILOTBYTE);
		j=0;
		while (!timeoutflag && j<8) {
			if(cb.count < cbcountmin) cbcountmin = cb.count;
			if ((millis() - timeout) > 5000) {
				Serial.printf("\nprg2turbo Pilot 503 timeout..");
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
	}
	if (PO) Serial.printf("End Pilot 503\n");
	if (PO) Serial.printf("cbcountmin %i\n\n", cbcountmin);


	timeout=millis();
	for (i=0x09; i>0x00; i--) { // 2nd countdown seq
		byte2turbo(bitsequence, i);
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 200) {
				Serial.printf("\nprg2turbo 2nd countdown sequence..");
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
	}
	if (PO) Serial.printf("End 2nd countdown sequence\n");

	timeout = millis();
	byte2turbo(bitsequence, 0); // filetype 0=DATA
	j=0;
	while (!timeoutflag && j<8) {
		if ((millis() - timeout) > 200) {
			Serial.printf("\nprg2turbo timeout filetype=0..");
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
	if (PO) Serial.printf("End filetype 0=DATA\n");

	prgchecksum = 0;
	timeout = millis();
	cbcountmin = BUFFER_SIZE;
	for (i=0; i<C64PRG.size()-2; i++) { // the actual DATA
		PRGbyte = C64PRG.read();
		prgchecksum^=PRGbyte; //need to do this only one round but this is easier
		byte2turbo(bitsequence, PRGbyte);
		j=0;
		while (!timeoutflag && j<8) {
			if(cb.count < cbcountmin) cbcountmin = cb.count;
			if ((millis() - timeout) > 200) {
				Serial.printf("\nprg2turbo DATA timeout..");
				timeoutflag = 1;
				Serial.printf("cbcount: %i\n", cb.count);
			}
			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				push_ringbuffer_twice(&cb, bitsequence[j]);
				#if TAPOUTPUT
					Serial.write(bitsequence[j]);
				#endif
				j++;
				timeout=millis(); // need to reset cause filesize is unknown
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
	byte2turbo(bitsequence, prgchecksum); // checksum
	j=0;
	while (!timeoutflag && j<8) {
		if ((millis() - timeout) > 200) {
			Serial.printf("\nprg2turbo checksum..");
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



	timeout = millis();
	byte2turbo(bitsequence, TRAILERBYTE);
	for (i=0; i<255; i++) { // trailer
		j=0;
		while (!timeoutflag && j<8) {
			if ((millis() - timeout) > 2000) {
				Serial.printf("\nprg2turbo timeout 255 trailer byte..");
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
	}
	if (PO) Serial.printf("End Trailer 255\n");
	
    //if (PO) Serial.printf("Closing file %s\n", C64filename);
    //C64PRG.close();

	// wait for all pulses..
	timeout = millis();
//	while (!timeoutflag && cb.count && digitalRead(TAPEMOTOR)) { //sync sequence short
	while (!timeoutflag && digitalRead(TAPEMOTOR)) { //sync sequence short
		if ((millis()-timeout) >2000) {
			Serial.printf("\nprg2turbo Timeout closing pulses..");
			timeoutflag = 1;
		}
		if (PO) Serial.printf("Sending pulses from buffer until motor stops.... %i\n", cb.count);
	}


/*	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	Serial.printf("WiFi Reconnecting: ");
  	while (WiFi.status() != WL_CONNECTED) {
    	delay(500);
    	Serial.print(".");
  	}
  	Serial.printf("\nWiFi Reconnected\n");
	Serial.printf("IP: %s\n\n", WiFi.localIP().toString().c_str());
*/
	 
	
}



