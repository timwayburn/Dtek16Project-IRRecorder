/* mipslabwork.c

   This file written 2015 by F Lundevall

   This file should be changed by YOU! So add something here:

   This file modified 2015-12-24 by Ture Teknolog

   Latest update 2015-08-28 by F Lundevall

   For copyright and licensing, see file COPYING */

#include <stdint.h>   /* Declarations of uint_32 and the like */
#include <pic32mx.h>  /* Declarations of system-specific addresses etc */
#include "mipslab.h"  /* Declatations for these labs */
#include <string.h>

int mytime = 0x5957;
int prime = 1234567;
int pulseWidth = 0;
int repeatCounter = 0;
unsigned int dutyC = 0;

volatile int* trise = (volatile int*) 0xbf886100; /* Define port using volatile pointer */
volatile int* porte = (volatile int*) 0xbf886110;
volatile int* t3con = (volatile int*) 0xbf800a00;
volatile int* tmr3 = (volatile int*) 0xbf800a10;
volatile int* pr3 = (volatile int*) 0xbf800a20;
int timeoutcount = 0;
int milli = 0;

char textstring[] = "text, more text, and even more text!";
unsigned int address = 0x00;
unsigned int addressComp = 0x00;
unsigned int command = 0x00;
unsigned int commandComp = 0x00;
unsigned int lastValidSignal = 0;

char a1[50] = "";
char ac1[50] = "";
char c1[50] = "";
char cc1[50] = "";

void display();
void decodeData();
void display2();
void display3();
void genPWM(int dutycycle);
void clrPWM();
void sendIR();

/* Interrupt Service Routine */
// void __attribute__((interrupt(IPL7))) __attribute__((vector(8))) user_isr(void) {
void user_isr( void ) {

	if (IFS(0) & 0x100) {
		timeoutcount++;
		if(timeoutcount>10){
			*porte += 1;
			timeoutcount = 0;
			sendIR(0xaaaaaaaa);
			IECCLR(0) = 0x100; //Disable interrupt TMR2
		}
		IFSCLR(0) = 0x100; // reset T2IF event flag

	}
	//Switch
	if (IFS(0) & 0x800) {
		timeoutcount = 0;
		*porte = 0x0;
		clrPWM();
		dutyC = 0;
		IFSCLR(0) = 0x800; // reset T2IF event flag
	}

	//Pin #2 RD8
	if (IFS(0) & 0x80) {
		TMR2 = 0x0; //clear
		while (!(PORTD & 0x100)); // While pulse burst, pin is low
		pulseWidth = TMR2;
		TMR2 = 0;
		//Starting 9ms AGC burst
		if ((pulseWidth > 2700 && pulseWidth < 2850)) {

			*porte += 1;

			while ((PORTD & 0x100)); // While pause, pin is high
			pulseWidth = TMR2;
			TMR2 = 0;
			//Followed by 4.5ms pause
			if ((pulseWidth > 1350 && pulseWidth < 1450)) {
				decodeData();
				display2();
				IFSCLR(0) = 0x80; // reset T1IF event flag
				return;
			} else if (pulseWidth > 625 && pulseWidth < 775) { //Potential repeat signal if 2.25ms pause
				while (!(PORTD & 0x100));
				pulseWidth = TMR2;
				TMR2 = 0;
				if (pulseWidth > 160 && pulseWidth < 190) { //Confirmed repeat 560us pulse
					//0xFFFF used as repeat
					while ((PORTD & 0x100));
					pulseWidth = TMR2;
					if ((pulseWidth > 28000) && (lastValidSignal != 0x0)) { // Should be around 95ms pause for repeat, check if its over 90ms
						repeatCounter++;
						if (repeatCounter % 3 == 0) {
							display_string( 1, "REPEATING.");
						} else if (repeatCounter % 3 == 1) {
							display_string( 1, "REPEATING..");
						} else if (repeatCounter % 3 == 2) {
							display_string( 1, "REPEATING...");
						}
						display2();
						IFSCLR(0) = 0x80; // reset T1IF event flag
						return;
					}

				}
			}
		}

		lastValidSignal = 0;

		IFSCLR(0) = 0x80; // reset T1IF event flag
		return;
	}
}

//Used for retreiving pulse duration for the timer
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

//Displays decoded data as integers
void display() {

	display_string( 0, itoaconv(address) );
	display_string( 1, itoaconv(addressComp) );
	display_string( 2, itoaconv(command)  );
	display_string( 3, itoaconv(commandComp) );

	display_update();
}

//Displays the decoded button pressed
void display2() {

	//3x8 bit code, non complemented address bits are not used.
	unsigned int value = (address << 24) + (addressComp << 16) + (command << 8) + commandComp;

	switch (value) {

	case 0xFF629D: display_string( 0, "FORWARD"); break;
	case 0xFF22DD:
		lastValidSignal = 0xFF22DD;
		display_string( 0, "LEFT");
		break;
	case 0xFF02FD:
		display_string( 0, "OK");
		IECSET(0) = 0x100; //Disable interrupt TMR2
		break;
	case 0xFFC23D:
		lastValidSignal = 0xFFC23D;
		display_string( 0, "RIGHT");
		break;
	case 0xFFA857: display_string( 0, "REVERSE"); break;
	case 0xFF6897:
		lastValidSignal = 0xFF6897;
		display_string( 0, "1");
		genPWM(50);
		break;
	case 0xFF9867:
		lastValidSignal = 0xFF9867;
		display_string( 0, "2");
		genPWM(1000);
		break;
	case 0xFFB04F:
		lastValidSignal = 0xFFB04F;
		display_string( 0, "3");
		genPWM(2000);
		break;
	case 0xFF30CF: display_string( 0, "4");    break;
	case 0xFF18E7: display_string( 0, "5");    break;
	case 0xFF7A85: display_string( 0, "6");    break;
	case 0xFF10EF: display_string( 0, "7");    break;
	case 0xFF38C7: display_string( 0, "8");    break;
	case 0xFF5AA5: display_string( 0, "9");    break;
	case 0xFF42BD: display_string( 0, "*");    break;
	case 0xFF4AB5: display_string( 0, "0");    break;
	case 0xFF52AD: display_string( 0, "#");    break;

	default:
		display_string( 0, "ERROR"); break;
	}

	display_update();
	display3();
	delay(1); // Do not get immediate repeat
}

//Alters LED brightness
void display3() {

	//3x8 bit code, non complemented address bits are not used.
	unsigned int value = (addressComp << 16) + (command << 8) + commandComp;

	switch (value) {
	case 0xFF22DD:
		dutyC--;
		genPWM(dutyC);
		break;
	case 0xFFC23D:
		dutyC++;
		genPWM(dutyC);
		break;
	}

	display_update();
	delay(300); // Do not get immediate repeat
}

//Decode 4x8bit signal data
void decodeData() {
	address = 0x00;
	addressComp = 0x00;
	command = 0x00;
	commandComp = 0x00;
	int i;
	//evaluate 8bit address
	for (i = 0; i < 8; i++) {
		if (determineBit()) { // If bit is 1, set adress bit to 1
			address |= (1 << (7 - i)); // (Signal data msb is sent first)
		}
	}
	//evaluate 8bit address complement
	for (i = 0; i < 8; i++) {
		if (determineBit()) {
			addressComp |= (1 << (7 - i));
		}
	}
	//evaluate 8bit command
	for (i = 0; i < 8; i++) {
		if (determineBit()) {
			command |= (1 << (7 - i));
		}
	}
	//evaluate 8bit command complement
	for (i = 0; i < 8; i++) {
		if (determineBit()) {
			commandComp |= (1 << (7 - i));
		}
	}
}

//Return 0 or 1 depending on duration of one 560us low pulse followed by one 560us or 1.7ms high.
//Return -1 if signal is bad
int determineBit() {
	TMR2 = 0;
	while (!(PORTD & 0x100)); //Wait while low
	pulseWidth = TMR2;
	TMR2 = 0;
	if (pulseWidth > 160 && pulseWidth < 190) { //562.5 microsec burst
		while (PORTD & 0x100);
		pulseWidth = TMR2;
		TMR2 = 0;
		if (pulseWidth > 160 && pulseWidth < 190) { //562.5 microsec pause  -> logical 0
			return 0;
		} else if (pulseWidth > 490 && pulseWidth < 560) { // 1.6875ms pause -> logical 1
			return 1;
		}
	}

	return -1;

}

void genPWM(int dutycycle) { //dutycyle of 2105 will generate 100% high
	OC4RS = dutycycle;
	OC4CON |= 0x8000; // set ON bit 1000 0000 0000 0000
}

void setDutyCycle(int dutycycle) {
	OC4RS = dutycycle;
}

void setPWM() { //dutycyle of 2105 will generate 100% high
	OC4CON |= 0x8000; // set ON bit 1000 0000 0000 0000
}

void clrPWM() {
	OC4CON &= 0x7fff; // Clear ON bit 1000 0000 0000 0000
}

void sendIR(unsigned int signal){ //0xFFEW151E
	setDutyCycle(862); // Set a duty cycle of 41% for 1.35V output
	int i;
	int bit;
	//AGC Burst 9ms
	*tmr4 = 0x0; //clear
	setPWM();
	while (*tmr4 < 2800); // 9ms burst
	*tmr4 = 0x0;
	clrPWM();
	//Pause 4.5ms
	while (*tmr4 < 1400); // Wait
	*tmr4 = 0x0;
	for(i=31; i>=0; i--){
		setPWM();
		bit = 0x1 & (signal >> i); //Move through all 32 bits starting from MSB
		while(*tmr4 < 175); // 562.5 us pulse burst, for both 1 and 0
		*tmr4 = 0x0;
		clrPWM();

		if(bit){ // if bit is a 1
			while(*tmr4 < 530); // 1.6875ms space
			*tmr4 = 0x0;
		} else{
			while(*tmr4 < 175); // 562.5us space
			*tmr4 = 0x0;
		}
	}
  //signal end
	setPWM();
	while(*tmr4 < 175); // 562.5us burst signal end
	clrPWM();

}

/* Lab-specific initialization goes here */
void labinit( void )
{
	//Timer2 (used for IR-reciever)
	IPCSET(2) = 0x1c; //0001 1100 ; set T2IP to 111, sets interrupt priority to 7
	IECCLR(0) = 0x100; // set T2IE to 1, enabling interrupts from TMR2
	IFSCLR(0) = 0x100; // reset T2IF event flag

	T2CONSET = 0x70; //0111 0000 set prescale to 256
	TMR2 = 0x0; //clear
	PR2 =  60000; // period should be 1/1000 second, 1ms/tick
	T2CONSET = 0x8000; // set ON bit, 1000 0000 0000 0000


	//Timer3 (used for PWM output pin #9, OC4)

	*t3con &= 0x8f; // AND 1000 1111 set prescale to 000
	*tmr3 = 0x0; //clear
	*pr3 =  2105; // 38 khz frequency, 80 000 000 / 38 000
	*t3con |= 0x8000; // set ON bit, 1000 0000 0000 0000

	//Timer4

	*t4con &= 0x70; // AND 1000 1111 set prescale to 000
	*tmr4 = 0x0; //clear
	*pr4 =  60000; // 38 khz frequency, 80 000 000 / 38 000
	*t4con |= 0x8000; // set ON bit, 1000 0000 0000 0000

	//PWM output OC4 Pin #9
	OC4CON |= 0x6; // set PWM mode OCM is 110
	OC4CON |= 0x8; // Select timer3 OCTSEL is 1000
	OC4CON &= 0x7fff; // Clear ON bit 1000 0000 0000 0000

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
	TRISDSET = 0xfe0; // Set bit 11 to 5 as inputs  1111 1110 0000

	TRISDCLR = 0x8;
	PORTDCLR = 0x8;

	return;
}

/* This function is called repetitively from the main program */
void labwork( void ) {
}
