/*
Written by: Thomas Wang and Tim Wayburn
Last Edited: 30/11/16
 */

#include <stdint.h>   /* Declarations of uint_32 and the like */
#include <pic32mx.h>  /* Declarations of system-specific addresses etc */
#include "mipslab.h"  /* Declatations for these labs */
#include <string.h>

// Define ports using volatile pointers
volatile int* trise = (volatile int*) 0xbf886100;
volatile int* porte = (volatile int*) 0xbf886110;
volatile int* t3con = (volatile int*) 0xbf800a00;
volatile int* tmr3 = (volatile int*) 0xbf800a10;
volatile int* pr3 = (volatile int*) 0xbf800a20;

int timeoutcount = 0;
int repeatslot = 0;
int pulseWidth = 0;
int repeatCounter = 0;
int recording = 0;

unsigned int address = 0x00;
unsigned int addressComp = 0x00;
unsigned int command = 0x00;
unsigned int commandComp = 0x00;
unsigned int lastValidSignal = 0;

unsigned int pulse[10][100] = {0};
unsigned int pause[10][100] = {0};

void decodeData();
void display2();
void sendIR();
void clrPWM();
void NECdecode();
void durationDecode(int j);

/* Interrupt Service Routine */
void user_isr( void ) {
	//Timer 2 Interrupt, sends IR signal after delay
	if ((IFS(0) & 0x100) && (IEC(0) & 0x100)) {
		*porte += 1;
		timeoutcount++;
		if (timeoutcount > 10) {
			sendIR();
			timeoutcount = 0;
			IECCLR(0) = 0x100; //Disable interrupt TMR2
		}
		IFSCLR(0) = 0x100; // reset T2IF event flag

	} else if
	//Switch Interrupt
	(IFS(0) & 0x800) {
		IFSCLR(0) = 0x800; // reset T2IF event flag
	} else if

	//Pin #2 RD8 Interrupt, IR-receiver receives signal
	(IFS(0) & 0x80) {

		NECdecode(); // Check for valid NEC signal
		display_update();
		int j = 0;
		int i;
		int timeout;

		delay(500);


		while (recording) {
			timeout = 0;
			display_string( 0, "RECORDING" );
			display_string( 1, "" );
			display_string( 2, "" );
			display_string( 3, "" );
			display_update();

			while ((PORTD & 0x100)); //Wait for incoming IR-signal
			TMR2 = 0x0; //clear
			for (i = 0; (i < 100) && !timeout; i++) { // For each pulse & pause
				while (!(PORTD & 0x100));
				pulseWidth = TMR2; //Pulse duration
				TMR2 = 0;
				pulse[j][i] = pulseWidth; // Insert pulse or pause in array
				while ((PORTD & 0x100)) {
					if (TMR2 > 30000) { //If pause timeout (signal has ended)
						pause[j][i] = 0; //0 interval indicates end of signal
						pulse[j][i + 1] = 0; //0 interval indicates end of signal
						if (i == 35) { //NEC signal, potential "end record"

							durationDecode(j); // Decode last signal as NEC signal, potential "end record"

							if (!recording) { // If end record
								repeatslot = j - 1; // Indicate where last non-end signal is located in array
								display_string( 0, "RECORDING ENDED" );
								display_string( 1, "" );
								display_string( 2, "" );
								display_string( 3, "" );
								display_update();
								IFSCLR(0) = 0x80; // reset T1IF event flag
								return;
							}
						}
						timeout = 1; // end for loop
						break; // end while loop
					}
				}

				if (!timeout) { //Only set pause if not timeout
					pulseWidth = TMR2; //Pause duration
					TMR2 = 0;
					pause[j][i] = pulseWidth;
				}
			}
			display_string( 0, "SIGNAL RECORDED" );
			display_string( 1, itoaconv( pulse[j][0] ) );
			display_string( 2, itoaconv( i ) );
			display_string( 3, itoaconv( j ) );
			display_update();

			j++;
			delay(1000);
		}

		IFSCLR(0) = 0x80; // reset T1IF event flag
		return;

	}
}

/* NEC protocol decode using stored durations, j is the array row */
void durationDecode(int j) {
	//AGC burst
	if ((pulse[j][0] > 2700 && pulse[j][0] < 2850)) {
		if (pause[j][0] > 1350 && pause[j][0] < 1450) {
			int i;
			address = 0x00;
			addressComp = 0x00;
			command = 0x00;
			commandComp = 0x00;

			for (i = 1; i <= 32; i++) {
				if (pause[j][i] > 490 && pause[j][i]  < 560) {
					commandComp |= (1 << (31 - (i - 1))); // Store all signal data in commandComp
				}
			}
			display2();
		}
	}
}

/*Attempts to decode incoming signal as a NEC signal, then displays determined signal on display*/
void NECdecode() {
	TMR2 = 0x0;
	while (!(PORTD & 0x100)); // While pulse burst, pin is low
	pulseWidth = TMR2; //Save duration of initial pulse
	TMR2 = 0;
	//Check starting 9ms AGC burst
	if ((pulseWidth > 2700 && pulseWidth < 2850)) {

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
					if (repeatCounter % 3 == 1) {
						display_string( 1, "REPEATING.");
					} else if (repeatCounter % 3 == 2) {
						display_string( 1, "REPEATING..");
					} else if (repeatCounter % 3 == 0) {
						display_string( 1, "REPEATING...");
					}
					display2();
					IFSCLR(0) = 0x80; // reset T1IF event flag
					return;
				}

			}
		}
	}
}

//Displays the decoded button pressed or error if invalid
void display2() {

	//3x8 bit code, non complemented address bits are not used.
	unsigned int value = (addressComp << 16) + (command << 8) + commandComp;

	if (recording && (value != 0xFF02FD)) { // Only valid signal is "OK" while recording
		return;
	}

	lastValidSignal = value;
	switch (value) {

	case 0xFF629D:
		display_string( 0, "FORWARD");
		IECSET(0) = 0x100; //Enable interrupt TMR2
		break;
	case 0xFF22DD:
		display_string( 0, "LEFT");
		break;
	case 0xFF02FD:
		display_string( 0, "OK");
		if (recording) {
			recording = 0;
		} else {
			recording = 1;
		}

		break;
	case 0xFFC23D:
		display_string( 0, "RIGHT");
		break;
	case 0xFFA857: display_string( 0, "REVERSE"); break;
	case 0xFF6897:
		display_string( 0, "1");
		break;
	case 0xFF9867:
		display_string( 0, "2");
		break;
	case 0xFFB04F:
		display_string( 0, "3");
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
		display_string( 0, "ERROR");
		lastValidSignal = 0;
		break;

	}

	display_update();
	delay(1); // Do not get immediate repeat
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
		if (determineBit()) {
			address |= (1 << (7 - i)); //Signal data msb is sent first
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

	return -1;

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

void sendIR() {
	if (pulse[0][0] == 0) { // no signals to send
		IFSCLR(0) = 0x80; // reset receiver event flag
		return;
	}

	display_string( 0, "SENDING SIGNALS" );
	display_string( 1, "" );
	display_string( 2, "" );
	display_string( 3, "" );
	display_update();
	setDutyCycle(862); // Set a duty cycle of 41% for 1.35V output
	//AGC Burst 9ms
	int i;
	int j = 0;

	while (j <= repeatslot) { // For all recorded signals
		display_string( 1, itoaconv(j) );
		display_update();
		for (i = 0; pulse[j][i] != 0; i++) { // For each pause/pulse (pulse != 0)
			TMR2 = 0x0; //clear
			setPWM(); // LED on
			while (TMR2 < pulse[j][i]);
			TMR2 = 0x0;
			clrPWM(); // LED off
			while (TMR2 < pause[j][i]);
		}
		j++; // Move onto next signal
		delay(2000); // delay between each signal to be sent
	}
	display_string( 0, "COMPLETE" );
	display_string( 1, "" );
	display_string( 2, "" );
	display_string( 3, "" );
	display_update();


	IFSCLR(0) = 0x80; // reset receiver event flag (Ignore signals while sending)
	return;

}

/* Lab-specific initialization goes here */
void labinit( void )
{
	//Timer2
	IPCSET(2) = 0x1c; //0001 1100 ; set T2IP to 111, sets interrupt priority to 7
	IECCLR(0) = 0x100; // set T2IE to 1, enabling interrupts from TMR2
	IFSCLR(0) = 0x100; // reset T2IF event flag


	T2CONSET = 0x70; //0111 0000 set prescale to 256
	TMR2 = 0x0; //clear
	PR2 =  60000; // period should be 1/1000 second, 1ms/tick
	T2CONSET = 0x8000; // set ON bit, 1000 0000 0000 0000

	//Timer3

	*t3con &= 0x8f; // AND 1000 1111 set prescale to 000
	*tmr3 = 0x0; //clear
	*pr3 =  2105; // 38 khz frequency, 80 000 000 / 38 000
	*t3con |= 0x8000; // set ON bit, 1000 0000 0000 0000

	//PWM output OC4 Pin #9
	OC4CON |= 0x6; // set PWM mode OCM is 110
	OC4CON |= 0x8; // Select timer3 OCTSEL is 1000
	OC4CON &= 0x7fff; // Clear ON bit 1000 0000 0000 0000

	//INT1 pin #2
	IPCSET(1) = 0x1c000000; //0001 1100 0000 0000 INT1IP 7
	IECSET(0) = 0x80; //1000 0000 INT1IE to 1
	IFSCLR(0) = 0x80;

	//INT2 Switch
	IPCSET(2) = 0x1c000000; //0001 1100 0000 0000 INT1IP 7
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

	//Initialize display

	display_string( 0, "WAITING 4 SIGNAL" );
	display_string( 1, "" );
	display_string( 2, "" );
	display_string( 3, "" );
	display_update();

	return;
}

/* This function is called repetitively from the main program */
void labwork( void ) {


}
