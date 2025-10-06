/*************************************************************************
 * 
 * void prg2tap(File C64PRG, char *header)
 * 
 * Assumes an open .PRG file in C64PRG
 * and a header[] array
 * 
 * converts a .PRG file to C64 (normal speed) Tape format
 * July 15 2025
 * 
 * Arjen Vellekoop
 * 
 * 
 * 
 * 
 * ***********************************************************/


/*	prg to tap
 normal CBM speed

 The different PAL pulse lengths and frequencies are:
 ● a short 182.7µs pulse (2737 Hz) --> $2F
 ● a medium 265.7µs pulse (1882Hz) --> $42
 ● a long 348.8µs pulse (1434 Hz) --> $57
 
● The bit value "0" is encoded as a short pulse period followed by a medium pulse period 	--> SM
● The bit value "1" is encoded as a medium pulse period followed by short pulse period		--> MS
● The byte marker is encoded as a long pulse period followed by a medium pulse period		--> LM
● The end-of-data marker is encoded as a long pulse period followed by a short pulse period	--> LS

 Byte Encoding
The byte marker 'LM' indicates the start of a byte. The bits are recorded with the least significant bit (LSB) first,
and a parity bit (odd parity) following the most significant bit (MSB).

[ByteMarker][LSB|Bit1|Bit2|Bit3|Bit4|Bit5|Bit6|MSB|OddParity]


	format:

- starts with 10secs of synchronisation pulses (short pulses) = 27367 pulses
- Then a countdown sequence $89,$88, ... $81
- followed by the header payload:

Header type (1 byte)
Start Address (2 bytes, LowHigh)
End Address (2 bytes, LowHigh)
Filename (16 bytes, padded with 0x20 = spaces)
Filename extra or payload (171 bytes)
1 byte Checksum EXOR --> followed by end of data marker EOD (longSmall)

		Header Type $01: Relocatable program. (BASIC)
		Header Type $02: DATA Denotes a data block of a sequential (ASCII) file. The bytes 1-192 (which is 191 bytes) contain the payload data.This block does not make use of the start and end address.
		Header Type $03: Non-relocatable programs. (Machine language)
		Header Type $04: Header of an ASCII file. Besides the header type, the payload of this block contains the file name.
		Header Type $05: End-of-tape block. In case the EOT is reached, before a header with the desired file name is reached, a "DEVICE NOT PRESENT ERROR" is reported.
		
- Another synchronisation sequence, 80 pulses Small (30msec)	
- Countdown sequence 2 	$09,$08, ... $01
- copy of the header payload

- synchronisation sequence 80 pulses
- Silence... the three bytes following $00 represent the exact number of clock cycles to the next pulse.
			about 0.3 secs coded as: $00 $05 $00 $05= 327685us	
			
			
- synchronisation sequence 5376 pulses ca 3sec (0x1500)

repeat...
- countdown sequence $89,$88, ... $81
- payload 192 bytes  --> In practice the entire PRG bytes are sent here. Can be (much) larger than 192
- 1 byte checksum --> followed by end of data marker EOD (longSmall)
- sync 80 pulses
- Countdown sequence 2 	$09,$08, ... $01
- copy 1st payload
- 1 byte checksum --> followed by end of data marker EOD (longSmall)
- sync 80 pulses

- repeat from countdown 89-88---81

*/


#include "main.h"


#define FILETYPE 0x03 // non relocatable program, machine language


void byte2cbm(uint8_t * bitstring, uint8_t byte) {
	// [ByteMarker][LSB|Bit1|Bit2|Bit3|Bit4|Bit5|Bit6|MSB|OddParity]
	int i, parity=0;
	bitstring[0] = CBMPULSE_LONG;
	bitstring[1] = CBMPULSE_MEDIUM;
	for (i=0; i<8; i++) {
		if (byte & (1<<i)) { // test bit
			bitstring[2*i+2] = CBMPULSE_MEDIUM;
			bitstring[2*i+3] = CBMPULSE_SHORT;
			//printf("1 ");
			parity++;
		}
		else {
			bitstring[2*i+2] = CBMPULSE_SHORT;
			bitstring[2*i+3] = CBMPULSE_MEDIUM;
			//printf("0 ");
		}
	}
	if (parity&1) {
		bitstring[2*i+2] = CBMPULSE_SHORT;
		bitstring[2*i+3] = CBMPULSE_MEDIUM;
		//printf(" 0 ");
	}
	else {
		bitstring[2*i+2] = CBMPULSE_MEDIUM;
		bitstring[2*i+3] = CBMPULSE_SHORT;
		//printf(" 1 ");
	}
}

void prg2tap(File C64PRG, char *header) {

	// debugging the output:
	#define TAPOUTPUT 0 // send to rs232

	uint8_t		prgchecksum, bitsequence[22];
	uint16_t i, j, k;
	uint32_t	tapsize;
	uint32_t	isync, isyncmax;
	bool		filerewind, timer1_running;

	#if TAPOUTPUT
	uint8_t tapheader[] = {0x43, 0x36, 0x34, 0x2d, 0x54, 0x41, 0x50, 0x45, 0x2d, 0x52, 0x41, 0x57, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t			silence[] = {0x00, 0x98, 0x01, 0x05}; // LowHigh format --> 328088 pulses
	#endif

	// Generate TAP file
	// Total file bytes:
	//
	// $6A00 = 27136 sync pulses
	// 4x countdown sequences each 9 bytes = 720 pulses
	// 4x EndOFData markers (EOD) = 8 pulses
	// sync $4F (79) for HEADER REPEATED and DATA REPEATED. 
	// 	Unclear whether this is required after the copy as well but for now we'll oblige
	// 	4x 79 = 316 pulses
	// After the header copy a silence: 4 pulses
	// Before the DATA sync: $1500 = 5376 pulses
	// 4x checksum = 80 pulses
	// The header = 192 bytes (twice) --> 7680 pulses
	// Plus actual data bytes (twice) *20
	//
	// formula: 41320 + 40*(fsize-2)
	//
	
	tapsize = 41320 + 40*(C64PRG.size()-2);

	// for tapheader ONLY WHEN SAVING TO TAP 
	#if TAPOUTPUT
		tapheader[16] = (tapsize&0xFF);
		tapheader[17] = (tapsize&0xFF00)>>8;
		tapheader[18] = (tapsize&0xFF0000)>>16;
		tapheader[19] = (tapsize&0xFF000000)>>24;
	#endif

	if (PO) tee.printf ("TAPsize:\t%i\n", tapsize);
	if (PO) tee.printf("This will take approximately %i sec\n", int(tapsize/2100)); // approximation
	#if TAPOUTPUT
		for (i=0; i<20; i++) { // TAP header
			Serial.write(tapheader[i]);
		}
	#endif

	timeout = millis(); // while loop. Always safe to add timeout
	timer1_running = false;
    timeoutflag = false;
	isync = 0;
	// 10 secs synchronisation..
	// use a variable CBMsyncLeader --> normally 0x6A00 = 27136 sync pulses (10secs)
	if (Uploadformat==2) isyncmax=CBMsyncLeader;
	else isyncmax = 0x6A00; // it's the default

	while(!timeoutflag && isync<isyncmax) { // 27136 sync pulses push to ringbuffer... (note: make this a variable to choose from)		
		if ((millis() - timeout) > 15000) { // i.e approx 10 secs of sync pulses.
        	tee.printf("\nprg2tap Stuck in isync while loop..   Aborting\n\n");
        	timeoutflag = true;
      	}
		if (cb.count < BUFFER_SIZE-1) { //push next byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
			push_ringbuffer_twice(&cb, CBMPULSE_SHORT);
			isync++;    
			#if TAPOUTPUT
				Serial.write(CBMPULSE_SHORT);
			#endif
		}
		else {
        	if (!timer1_running) {
          		timer1_write(5000); // start the timer
          		timer1_running = 1;
				tee.printf("Started the timer. in isync\n\n");
        	}
		}
		yield(); // required in while loop
		if (PO) {if (!(isync%100)) Serial.printf("%i --> %i\r", 0x6A00, isync);}
	}
	
	if (PO) Serial.printf("\n\nEnd 10secs synchronisation\n");


	if (!timer1_running) { // When the buffer did not fill...
   		timer1_write(5000); // start the timer
   		timer1_running = 1;
		//cbcountmin = BUFFER_SIZE;
		tee.printf("Started the timer. After TurbosyncLeader\n\n");
	}



	timeout = millis(); // reset timeout
	k = 0;

	// some debugging
	    if (PO) Serial.printf("TapeSense: %i\n", digitalRead(TAPESENSE));
        if (PO) Serial.printf("cb.count: %i\n", cb.count);
	// end debugging

	// Now send the header followed by 80 pulses sync
	// twice with a countdown sequence of 0x89-0x88---0x81  and (2nd time) 0x09-0x08----0x01

	while(!timeoutflag && k<2) { // send the header (twice)
		for (i=0x89-k*0x80; i>0x80-k*0x80; i--) { // 1st countdown seq
			byte2cbm(bitsequence, i);
			j=0;
			while(!timeoutflag && j<20) {
				if ((millis() - timeout) > 1000) { 
		       		Serial.printf("\nprg2tap Stuck in countdown sequence while loop..   Aborting\n\n");
        			timeoutflag = true;
      			}

				if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
					push_ringbuffer_twice(&cb, bitsequence[j]);
					j++;     
					#if TAPOUTPUT
						Serial.write(bitsequence[j]);
					#endif
				}
			}
			if (PO) Serial.printf("countdown header: %x\n", i);
		}
		if (!timeoutflag) timeout=millis(); // reset timeout
		for (i=0; i<193; i++) { // Send the acual header data
			byte2cbm(bitsequence, header[i]);
			j=0;
			while(!timeoutflag && j<20) {
				if ((millis() - timeout) > 1000) { 
		       		Serial.printf("\nprg2tap Stuck in countdown sequence while loop..   Aborting\n\n");
        			timeoutflag = true;
      			}

				if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
					push_ringbuffer_twice(&cb, bitsequence[j]);
					j++;    
					#if TAPOUTPUT
						Serial.write(bitsequence[j]);
					#endif
					timeout = millis(); // reset 
				}
				yield(); //reset WDT
			}
		}
		if (PO) Serial.printf("Sent 192 bytes header %i\n", i);


		if (!timeoutflag) timeout=millis(); // reset timeout
		while (!timeoutflag && cb.count > BUFFER_SIZE-4) { // need space for 4 pushes EOD
			if ((millis()-timeout) > 100) {
	       		Serial.printf("\nprg2tap Stuck in EOD marker sequence while loop..   Aborting\n\n");
   				timeoutflag = true;
			}
		}
		if (!timeoutflag) { //push next 2 bytes EOD= LONG-SHORT
			push_ringbuffer_twice(&cb, CBMPULSE_LONG);
			#if TAPOUTPUT
				Serial.write(CBMPULSE_LONG);
			#endif

			push_ringbuffer_twice(&cb, CBMPULSE_SHORT);
			#if TAPOUTPUT
				Serial.write(CBMPULSE_SHORT);
			#endif

		}
		if (PO) Serial.printf("pushed EOD\n");


		if (!timeoutflag) timeout=millis(); // reset timeout
		i=0;
		while (!timeoutflag && i<80-1) { // 80 pulses sync LET OP EVEN 79
			if ((millis()-timeout) > 1000) {
	       		Serial.printf("\nprg2tap Stuck in endsync1 sequence loop..   Aborting\n\n");
   				timeoutflag = true;
			}

			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				push_ringbuffer_twice(&cb, CBMPULSE_SHORT);
				#if TAPOUTPUT
					Serial.write(CBMPULSE_SHORT);
				#endif
				i++;
				timeout = millis();
			}
			yield();
		}
		if (PO) Serial.printf("End of endsync pulses 80\n");
		k++;
	}

	// wait for the motor to stop THIS IS TRICKY!!
	if (PO) Serial.printf("Wait for the motor to stop:\n");
	i=0;
	timeout=millis();
	while (!timeoutflag && digitalRead(TAPEMOTOR)) {
		if ((millis()-timeout) > 3000) {
       		Serial.printf("\nprg2tap Motor did not stop?   Aborting\n\n");
			timeoutflag = true;
		}
		if (PO) Serial.printf("Wait for the motor to stop: %i  %i\r", i++, cb.count);
	}

	// Do we need a 'silence' here?


	timeout = millis();
	if (PO) tee.printf("Waiting for motor voltage...\n");
	i=0;
	while(!timeoutflag && !digitalRead(TAPEMOTOR)) {
		if ((millis()-timeout) > 20000) { // should start within 20secs
       		tee.printf("\nprg2tap Motor did not start within 20secs..   Aborting\n\n");
			timeoutflag = true;
		}
		yield();
			
		if(!(i%1000)) {if (PO) Serial.printf("Waiting for motor voltage... %i\r", i);}
		i++;
	}
	if (PO) Serial.printf("\nEnd motor while loop...\n");

	#if TAPOUTPUT
		for (i=0; i<4; i++) {
			Serial.write(silence[i]);
		}
	#endif
	if (!timeoutflag) { // start timer1
		timer1_write(1640440); // 328msec we need a 'silence' here 328msec
		if (PO) tee.printf("Timer1 started 300msec\n");
	}
	// sync pulses after the 1st header CHECK THIS IN THE SPECS!!
	i=0;
	timeout = millis();
	while (!timeoutflag && i<0x1500) {
		if ((millis()-timeout) > 2000) {
   			tee.printf("\nprg2tap Stuck in start sync before PRG data..   Aborting\n\n");
   			timeoutflag = true;
		}

		if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
			push_ringbuffer_twice(&cb, CBMPULSE_SHORT);
			#if TAPOUTPUT
				Serial.write(CBMPULSE_SHORT);
			#endif
			i++;
			timeout = millis();
		}
		yield();
	}
	if (PO) Serial.printf("End of PRG start sync pulses 1500\n");
	if (PO) Serial.printf("TapeSense: %i\n", digitalRead(TAPESENSE));
    if (PO) Serial.printf("cb.count: %i\n", cb.count);


	// now for the real payload...
	if (!timeoutflag) timeout=millis(); // reset timeout
	k=0;
	while(!timeoutflag && k<2) { // send program block twice
		for (i=0x89-k*0x80; i>0x80-k*0x80; i--) { // 1st countdown seq
			byte2cbm(bitsequence, i);
			j=0;
			while(!timeoutflag && j<20) {
				if ((millis() - timeout) > 1000) { 
		       		Serial.printf("\nprg2tap Stuck in PRG countdown sequence..   Aborting\n\n");
        			timeoutflag = true;
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
			if (PO) Serial.printf("countdown PRG header: %x\n", i);
		}

		if (PO) Serial.printf("Now read the program file bytes..\n");
		if (PO) Serial.printf("Motor: %i\n", digitalRead(TAPEMOTOR));
		if (PO) Serial.printf("CBcount: %i\n", cb.count);

		prgchecksum = 0;
		timeout = millis();
		for (i=0; i<C64PRG.size()-2; i++) { //1st copy DATA  first 2 bytes were holding startaddress
			PRGbyte = C64PRG.read();
			prgchecksum^=PRGbyte; //need to do this only one round but this is easier
			byte2cbm(bitsequence, PRGbyte);
			j=0;
			while(!timeoutflag && j<20) {
				if ((millis() - timeout) > 10000) { 
		       		tee.printf("\nprg2tap Stuck in program sequence loop..   Aborting\n\n");
					tee.printf("cbcount: %i\n", cb.count);
					tee.printf("i, j: %i, %i\n", i, j);
        			timeoutflag = true;
      			}

				if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
					push_ringbuffer_twice(&cb, bitsequence[j]);
					#if TAPOUTPUT
						Serial.write(bitsequence[j]);
					#endif
					j++;     
					timeout = millis(); // need to reset because Program size is unknown
				}
				yield();
			}
			if (!(i%100)) {
				if (PO) Serial.printf("PRGbyte: %i --> %i byte: %02x, CBcount: %i\n", C64PRG.size()-2, i, PRGbyte, cb.count);
			}

		}
		if (PO) Serial.println();

		byte2cbm(bitsequence, prgchecksum); // checksum
		j=0;
		while(!timeoutflag && j<20) {
			if ((millis() - timeout) > 1000) { 
			   	tee.printf("\nprg2tap Stuck in checksum sequence loop..   Aborting\n\n");
        		timeoutflag = true;
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
		if (PO) Serial.printf("Sent checksum %02X\n", prgchecksum);

		if (!timeoutflag) timeout=millis(); // reset timeout
		while (!timeoutflag && cb.count > BUFFER_SIZE-4) { // need space for 4 pushes
			if ((millis()-timeout) > 100) {
	       		tee.printf("\nprg2tap Stuck in last EOD marker sequence while loop..   Aborting\n\n");
   				timeoutflag = true;
			}
		}
		if (!timeoutflag) { //push next 2 bytes
			push_ringbuffer_twice(&cb, CBMPULSE_LONG);
			#if TAPOUTPUT
				Serial.write(CBMPULSE_LONG);
			#endif

			push_ringbuffer_twice(&cb, CBMPULSE_SHORT);
			#if TAPOUTPUT
				Serial.write(CBMPULSE_SHORT);
			#endif
		}
		if (PO) Serial.printf("Sent PRG EOD marker..\n");

		if (!timeoutflag) timeout=millis(); // reset timeout
		i=0;
		while (!timeoutflag && i<80-1) { // 80 pulses sync (actually 79)
			if ((millis()-timeout) > 1000) {
	       		tee.printf("\nprg2tap Stuck in endsync2 sequence loop..   Aborting\n\n");
   				timeoutflag = true;
			}

			if (cb.count < BUFFER_SIZE-1) { //push every byte twice to the ringbuffer. Check for at least 2 bytes in the ringbuffer.
				push_ringbuffer_twice(&cb, CBMPULSE_SHORT);
				#if TAPOUTPUT
					Serial.write(CBMPULSE_SHORT);
				#endif
				i++;
			}
			yield();
		}
		if (PO) Serial.printf("Sent sync sequence short (80).\n");
		k++;
		filerewind = C64PRG.seek(2, SeekSet); // rewind file to 2nd position (from 0)
		if (PO) Serial.printf("Rewind File: %i\n", filerewind);
		yield();

	}


	digitalWrite(TAPESENSE, 1);  // Required as to stop the motor in between with auto_turbo
	//Serial.printf("STOPPED THE TAPE HERE\n\n");

	// wait for all pulses..
	timeout = millis();
//	while (!timeoutflag && cb.count && digitalRead(TAPEMOTOR)) { //sync sequence short
	while (!timeoutflag && digitalRead(TAPEMOTOR)) { //sync sequence short
		if ((millis()-timeout) >2000) {
			tee.printf("\nprg2tap Timeout closing pulses..");
			timeoutflag = 1;
		}
		if (PO) Serial.printf("Sending pulses from buffer until motor stops.... %i\n", cb.count);
	}



}
