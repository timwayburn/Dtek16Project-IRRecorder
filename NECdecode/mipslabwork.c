/* mipslabwork.c

   This file written 2015 by F Lundevall

   This file should be changed by YOU! So add something here:

   This file modified 2015-12-24 by Ture Teknolog

   Latest update 2015-08-28 by F Lundevall

   For copyright and licensing, see file COPYING */

#include <stdint.h>   /* Declarations of uint_32 and the like */
#include <pic32mx.h>  /* Declarations of system-specific addresses etc */
#include "mipslab.h"  /* Declatations for these labs */
#include <stdio.h>
#include <string.h>

int mytime = 0x5957;
int prime = 1234567;
int pulseWidth = 0;
volatile int* trise = (volatile int*) 0xbf886100; /* Define port using volatile pointer */
volatile int* porte = (volatile int*) 0xbf886110;
int timeoutcount = 0;
int milli = 0;

char textstring[] = "text, more text, and even more text!";
char address[] = "00000000";
char addressComp[] = "00000000";
char command[] = "00000000";
char commandComp[] = "00000000";

/* Interrupt Service Routine */
// void __attribute__((interrupt(IPL7))) __attribute__((vector(8))) user_isr(void) {
void user_isr( void ) {

	if (IFS(0) & 0x100) {
		timeoutcount++;
		IFSCLR(0) = 0x100; // reset T2IF event flag

	}
	//Switch
	if (IFS(0) & 0x800) {
		timeoutcount = 0;
		*porte = 0x0;
		IFSCLR(0) = 0x800; // reset T2IF event flag
	}

	//Pin #2 RD8
	if (IFS(0) & 0x80) {
		TMR2 = 0x0; //clear
		while (!(PORTD & 0x100));
		pulseWidth = TMR2;
		TMR2 = 0;
		//No starting 9ms AGC burst
		if (!(pulseWidth > 2700 && pulseWidth < 2850)) {
			strncpy(address, "Error", 5);
			display();
			IFSCLR(0) = 0x80; // reset T1IF event flag
			return;
		}

		while ((PORTD & 0x100));
		pulseWidth = TMR2;
		TMR2 = 0;
		//No 4.5ms pause
		if (!(pulseWidth > 1350 && pulseWidth < 1450)) {
			strncpy(address, "Error", 5);
			display();
			IFSCLR(0) = 0x80; // reset T1IF event flag
			return;
		}

		decodeData();
		display();
		IFSCLR(0) = 0x80; // reset T1IF event flag
		return;
	}
}

void test() {
	TMR2 = 0;
	while (!(PORTD & 0x100)); //Wait while low
	pulseWidth = TMR2;
	TMR2 = 0;

	while (PORTD & 0x100);
	pulseWidth = TMR2;
	TMR2 = 0;

	while (!(PORTD & 0x100)); //Wait while low
	pulseWidth = TMR2;
	TMR2 = 0;

	while (PORTD & 0x100);
	pulseWidth = TMR2;
	TMR2 = 0;


}

void display() {

	display_string( 0, address );
	display_string( 1, addressComp );
	display_string( 2, command  );
	display_string( 3, commandComp );
	display_update();
	delay(1000);
	IFSCLR(0) = 0x80; // reset T1IF event flag
}

void decodeData() {

	int i;
	//evaluate 8bit address
	for (i = 0; i < 8; i++) {
		if (determineBit() == 1) {
			address[i] = '1';
		} else {
			address[i] = '0';
		}
	}

	//evaluate 8bit address complement
	for (i = 0; i < 8; i++) {
		if (determineBit() == 1) {
			addressComp[i] = '1';
		} else {
			addressComp[i] = '0';
		}
	}
	//evaluate 8bit command
	for (i = 0; i < 8; i++) {
		if (determineBit() == 1) {
			command[i] = '1';
		} else {
			command[i] = '0';
		}
	}
	//evaluate 8bit command complement
	for (i = 0; i < 8; i++) {
		if (determineBit() == 1) {
			commandComp[i] = '1';
		} else {
			commandComp[i] = '0';
		}
	}

}

int determineBit() {
	TMR2 = 0;
	while (!(PORTD & 0x100)); //Wait while low
	pulseWidth = TMR2;
	TMR2 = 0;
	if (pulseWidth > 160 && pulseWidth < 190) {
		while (PORTD & 0x100);
		pulseWidth = TMR2;
		TMR2 = 0;
		if (pulseWidth > 160 && pulseWidth < 190) {
			return 0;
		} else if (pulseWidth > 490 && pulseWidth < 560) {
			return 1;
		}
	}

}

/* Lab-specific initialization goes here */
void labinit( void )
{
	//Timer
	IPCSET(2) = 0x1c; //0001 1100 ; set T2IP to 111, sets interrupt priority to 7
	IECCLR(0) = 0x100; // set T2IE to 1, enabling interrupts from TMR2
	IFSCLR(0) = 0x100; // reset T2IF event flag

	T2CONSET = 0x8000; // set ON bit, 1000 0000 0000 0000
	T2CONSET = 0x70; //0011 0000 set prescale to 256
	TMR2 = 0x0; //clear
	PR2 =  60000; // period should be 1/1000 second, 1ms/tick


	//INT1 pin #2
	IPCSET(1) = 0x04000000; //0000 0100 0000 0000 INT1IP 1
	IECSET(0) = 0x80; //1000 0000 INT1IE to 1
	IFSCLR(0) = 0x80;

	//INT2 Switch
	IPCSET(2) = 0x04000000; //0001 1100 0000 0000 INT1IP 1
	IECSET(0) = 0x800; //1000 0000 0000 INT2IE to 1
	IFSCLR(0) = 0x800;
	enable_interrupt();

	//LED lamps
	*trise &= 0xFF00; // set porte bits, 1 as input, 0 as output
	*porte = 0x0; // clear PORTE

	//Switches and #pin 2
	PORTD |= 0xfe0; // Set bit 11 to 5 as inputs  1111 1110 0000

	return;
}

/* This function is called repetitively from the main program */
void labwork( void ) {

}

