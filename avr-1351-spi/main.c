/*
    - clocks and delays are for 8MHz crystal
    - joyport should occupy whole port (faster to clear)
    - WARNING! resetting mouse causes problems with noname mouse (MS Intellimouse works fine),
      possibly remove it (no hangup, just RMB is not correctly reported)
    - IMPROVEME:
	- on 1351 RMB is connected to POTX (for joystick emu)
*/
/*
    SPI:
    - always slave
    - PD3/INT1 is PS/2 CLK *and* SCK (PB7)
    - PB3 - set to output-low and routed to PB4 (so PB4 mustn't be disconnected for programming)
    - PB4(/SS) - slave select (must be grounded/routed from another port set to 0)
    - PB5(MOSI) is PS/2 DATA *read*
    - PB6(MISO) is PS/2 DATA *write*
    - PB7(SCK)  is PS/2 CLK
    - SPI is enabled by INT1 or sendbyte routine

    ISP-SPI
    WARNING: PB4(/SS) pin cannot be grounded when programming (don't care if routed from another port)
    WARNING: PB5 and PB6 cannot be shorted when programming (only switch/jumper can help here)
*/

#include <inttypes.h>
#include <sig-avr.h>
#include <interrupt.h>
#include <io.h>
#include <stdlib.h>
//#include <eeprom.h>
//#include <pgmspace.h>

#define FALSE  0
#define TRUE   (~FALSE)

/* input buffer size */
#define BUFF_SIZE	64

/* threshold values for joystick emulation mode - movements smaller than that are ignored */
#define XTHRESHOLD	5
#define YTHRESHOLD	5

/* definitions for joystick emulation */
#define JOYDDR		DDRA
#define JOYPORT		PORTA
#define JOYUP		PA0
#define JOYDOWN		PA1
#define JOYLEFT		PA2
#define JOYRIGHT	PA3
#define	JOYFIRE		PA4

/* PS/2 mouse commands */
#define	MSE_Reset		0xFF
#define MSE_Resend		0xFE
#define	MSE_SetDefaults 	0xF6
#define MSE_DisDataReporting	0xF5
#define MSE_EnDataReporting	0xF4
#define MSE_SetSampleRate	0xF3
#define MSE_GetDeviceID		0xF2
#define MSE_SetRemoteMode	0xF0
#define MSE_SetWrapMode		0xEE
#define MSE_ResetWrapMode	0xEC
#define MSE_ReadData		0xEB
#define MSE_SetStreamMode	0xEA
#define MSE_StatusRequest	0xE9
#define MSE_SetResolution	0xE8
#define MSE_SetScaling21	0xE7
#define MSE_SetScaling11	0xE6
/* PS/2 mouse return codes */
#define MSE_Error		0xFC
#define MSE_Ack			0xFA
#define MSE_BATPassed		0xAA

uint8_t edge;
uint8_t kb_buffer[BUFF_SIZE];
uint8_t *inpt, *outpt;
volatile uint8_t buffcnt, bitcount;
volatile uint8_t rdata, wdata, wparity, receiving;

volatile uint8_t delaydone;

void init_io (void) {
    /* setup lines for joystick emulation */
    outp(0x00, JOYDDR);
    /* if input - tristate, if output - pull down */
    outp(0x00, JOYPORT);
}

void init_ps2(void) {
    edge = 0;		// irq on falling edge
    receiving = 1;
    bitcount = 0;

    sbi(MCUCR, ISC11);	// setup irq on falling edge
    cbi(MCUCR, ISC10);
    // enable irqs
    sbi(GIMSK, INT1);
    sei();
    // setup port D clock line for INT1 (input, pull high)
    cbi(DDRD, PD3);
    sbi(PORTD, PD3);
    // setup port B for SPI interface
    outb(0x08, DDRB);	// PB3 - out
    outb(0x60, PORTB);	// data-o=high, data-i=pullhigh, /ss(PB4)==PB3(low)
}

void init_buffer(void) {
    /* initialize buffer */
    cli();
    inpt = kb_buffer;
    outpt = kb_buffer;
    buffcnt = 0;
    sei();
}

void put_kbbuff(uint8_t sc) {
    if (buffcnt < BUFF_SIZE) {
	*inpt = sc;
	++inpt;
	++buffcnt;
	if (inpt >= kb_buffer + BUFF_SIZE)
	    inpt = kb_buffer;
    }
}

uint8_t ps2_getbyte(void) {
uint8_t sc;
    while (buffcnt==0);
    sc = *outpt;
    ++outpt;
    if (outpt >= kb_buffer+BUFF_SIZE)
	outpt = kb_buffer;
    --buffcnt;
    return sc;
}

void delay (uint16_t time) {
    /* delay for time usec */
    outb(time >> 8, OCR1AH);
    outb(time & 0xff, OCR1AL);
    outb(0x0a, TCCR1B);	// prescale by 8, clear when matches A
    sbi(TIMSK, OCIE1A); // enable irq for Output Compare A
    delaydone = 0;
    while (delaydone == 0);
}

INTERRUPT (SIG_OUTPUT_COMPARE1A) {
    outb(0, TCCR1B);	// stop the counter
    cbi(TIMSK, OCIE1A);	// disable irq from Output Compare A
    delaydone = 0xff;
}

INTERRUPT (SIG_SPI) {
    if (receiving) {
	// fetch data but don't pass it to buffer yet
	rdata = inp(SPDR);
    } else {
	// notify that byte was sent - switch to receive mode
	++bitcount;
	edge = 0;
	cbi(MCUCR, ISC10);	// setup on falling edge
    }
    // enable INT1 irqs
    sbi(GIMSK, INT1);
    // disable SPI
    outp(0, SPDR);	// clear outputs
    outp(0, SPCR);
}

INTERRUPT (SIG_INTERRUPT1) {
    if (receiving) {
	if (!edge) { // falling edge
	    // ignore start, parity and stop bit
	    edge=1;
	    sbi(MCUCR, ISC10);	// setup on rising edge
	} else {     // rising edge
	    cbi(MCUCR, ISC10);	// setup on falling edge
	    edge=0;
	    if (bitcount==0) {
		// setup SPI
		// enable interrupt from SPI
		// CPHA=0, DORD=1, CPOL=1
		outb(0x80+0x40+0x20+0x08,SPCR);
		// disable interrupts from INT1
		cbi(GIMSK, INT1);
	    }
	    ++bitcount;
	    if (bitcount==3) {
		// last bit (stop) was just received
		put_kbbuff(rdata);
		bitcount=0;
	    }
	}
    } else {
	if (!edge) {
	    edge=1;
	    sbi(MCUCR, ISC10);	// setup on rising edge
	    switch(bitcount) {
	       case 0:	// startbit sent, push data into shift register
		// setup SPI: interrupt
		// DORD=1 CPHA=1 CPOL=1
		outb(0x80+0x40+0x20+0x08+0x04, SPCR);
		// put byte to send
		outb(wdata, SPDR);
		// disable interrupts from INT1
		cbi(GIMSK, INT1);
		break;
	       case 1:	// ignore one bogus interrupt - shot again
		edge=0;
		cbi(MCUCR, ISC10);	// setup on falling edge
	        ++bitcount;
	        break;
	       case 2:	// data sent, push parity bit
		if (wparity) sbi(PORTB,PB6); else cbi(PORTB,PB6); break;
	       case 3:	// parity sent, push stop bit
		sbi(PORTB,PB5);	//data input high
		sbi(PORTB,PB6);	//data output high
		sbi(PORTB,PB7);	//clock pullup
		cbi(DDRB,PB6);	//release data
		break;
//	       case 4:	// stop bit sent, ignore ack
//		break;
	    }
	} else {
	    edge=0;
	    cbi(MCUCR, ISC10);	// setup on falling edge
	    ++bitcount;
	    if (bitcount==5) {	// everything set, enter idle mode
		receiving = 1;
		bitcount = 0;
	    }
	}
    }
}

void ps2_sendbyte(uint8_t code) {
    while (bitcount!=0);	// wait for current transmission to end
    cbi(GIMSK, INT1);		// disable INT1 irqs
    receiving = 0;
    /* make CLK output and low */
    sbi(DDRB,PB7);
    cbi(PORTB,PB7);
    sbi(PORTB,PB6);
    delay(150);			/* wait 150us (about that, no less) */
    /* make DATA output and do startbit */
    cbi(PORTB,PB6);
    sbi(DDRB,PB6);
    /* make CLK input and open */
    cbi(DDRB,PB7);
    cbi(PORTB,PB7);
    sbi(GIMSK, INT1);		// will be active on falling edge
    wdata = code;
    /* prepare the bit that will be active when SPI thinks that last bit was sent */
    if (code & 0x80) sbi(PORTB, PB6); else cbi(PORTB, PB6);
    /* calculate parity bit */
    if (parity_even_bit(code)==0) wparity = 1; else wparity = 0;
    while (receiving==0);
}

void ps2_sendackcmd(uint8_t command) {
uint8_t state;
resend:
    ps2_sendbyte(command);
    state = ps2_getbyte();
    if (state == MSE_Ack)
	return;
    if (state == MSE_Resend)
	goto resend;
    outp(0xff, JOYDDR);
    outp(0x07, JOYPORT);	/// we're fucked
    while (TRUE);
}

int main(void) {
uint8_t id, state, deltax, deltay;

    init_io();		// for LED pins
    init_ps2();		// PS2 input buffer & port D init

    for (state=20;state>0;state--)
	delay(50000);	// wait 0.05s * 40 == 1s

    init_buffer();	// flush input buffer
    /* we're here after power-on RESET but we force another one */
    ps2_sendackcmd(MSE_Reset);

    state = ps2_getbyte();
    id = ps2_getbyte();

    if ( (state != MSE_BATPassed) || (id != 0) ) {
	outp(0xff, JOYDDR);
	outp(0x01, JOYPORT);	/// we're fucked
	return 0;
    }

    /* set samplerate to 60/s */
    ps2_sendackcmd(MSE_SetDefaults);
    ps2_sendackcmd(MSE_SetSampleRate);
    ps2_sendackcmd(60);
    /* read status of RMB */
    ps2_sendackcmd(MSE_ReadData);
    state = ps2_getbyte(); ps2_getbyte(); ps2_getbyte();
    if ( state & 0x02 ) {
	// RMB is pressed - enter joystick emulation mode
	/* now query in loop for data packets, no need to switch to RemoteMode */
	while (TRUE) {
	    delay(10000);	/* some delay to keep it about 60Hz */
	    ps2_sendackcmd(MSE_ReadData);
	    /* get data packet */
	    state = ps2_getbyte();
	    deltax= ps2_getbyte(); if (state & 0x10) deltax = 256-deltax;
	    deltay= ps2_getbyte(); if (state & 0x20) deltay = 256-deltay;
	    /* clear everything */
	    outp(0, JOYDDR);
	    /* button state - each of 3 buttons is fire */
	    if (state & 0x07) sbi(JOYDDR,JOYFIRE);
	    /* now each of 4 directions */
	    if (deltax>XTHRESHOLD) { if (state & 0x10) sbi(JOYDDR,JOYLEFT); else sbi(JOYDDR,JOYRIGHT); };
	    /* this is reversed to common sense - mouse treats 'down' as decreasing y position */
	    if (deltay>YTHRESHOLD) { if (state & 0x20) sbi(JOYDDR,JOYDOWN); else sbi(JOYDDR,JOYUP); };
	}
    } else {
	// RMB is released - enter 1351 emulation mode
	outp(0xff, JOYDDR);
	outp(0x03, JOYPORT);
	while (TRUE);
    }
}
