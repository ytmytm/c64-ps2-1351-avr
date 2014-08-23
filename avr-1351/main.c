
/*
    - clocks and delays are for 8MHz crystal (divide by 2 in delay())
    - PD3/INT1 is PS/2 CLK line, PD4 is PS/2 DATA line (latter may be changed)
    - joyport should occupy whole port (faster to clear)
    - WARNING! resetting mouse causes problems with noname mouse (MS Intellimouse works fine),
      possibly remove it (no hangup, just RMB is not correctly read)
    - IMPROVEME:
	- read and update internal X/Y pointers from mouse data
	- on each delay - if X/Y nonzero - enable lines, decrease |x| and/or |y|
	  by value; if zero - disable line
    - IMPROVEME:
	- on 1351 RMB is connected to POTX
    - IMPROVEME:
	- joystick lines should be pulled low when active (can be even done if
	  inputs?)
*/

#include <inttypes.h>
#include <sig-avr.h>
#include <interrupt.h>
#include <io.h>
//#include <eeprom.h>
//#include <pgmspace.h>

#define FALSE  0
#define TRUE   (~FALSE)

/* input buffer size */
#define BUFF_SIZE	64

/* threshold values for joystick emulation mode - movements smaller than that are ignored */
#define XTHRESHOLD	15
#define YTHRESHOLD	15

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

volatile uint8_t delaydone;

void init_io (void) {
    /* setup lines for joystick emulation */
    outp(0xff, JOYDDR);
    outp(0x00, JOYPORT);
}

void init_ps2(void) {
    edge = 0;		// irq on falling edge
    bitcount = 11;
    sbi(MCUCR, ISC11);	// setup irq on falling edge
    cbi(MCUCR, ISC10);
    // enable irqs
    sbi(GIMSK, INT1);
    sei();
    // setup port D data&clock line (input, pull high)
    cbi(DDRD, PD4);
    cbi(DDRD, PD3);
    sbi(PORTD, PD4);
    sbi(PORTD, PD3);
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

INTERRUPT (SIG_INTERRUPT1) {
static uint8_t data;
    if (!edge)	{ //entered on falling edge
	if ((bitcount<11) && (bitcount>2)) { // data bit incoming
	    data >>= 1;
	    if (bit_is_set(PIND,PD4))
		data |= 0x80;
	}
	edge = 1;
	sbi(MCUCR, ISC10);	// setup irq on rising edge
    } else {
	// entered on rising edge
	cbi(MCUCR, ISC10);	// setup irq on falling edge
	edge = 0;
	if (--bitcount == 0) {
	    // all bits received - put to input buffer
	    put_kbbuff(data);
	    bitcount = 11;
	}
    }
}

void ps2_sendbyte(uint8_t sc) {
uint8_t parity, i;
    parity = 0;
    while (bitcount!=11);	// wait for current transmission to end
    cbi(GIMSK, INT1);		// disable input irqs
    /* make CLK output and low */
    sbi(DDRD,PD3);
    cbi(PORTD,PD3);
    sbi(PORTD,PD4);
    delay(150);			// wait 150us
    /* make DATA output and do startbit */
    sbi(DDRD,PD4);
    cbi(PORTD,PD4);
    /* make CLK input and open */
    cbi(DDRD,PD3);
    cbi(PORTD,PD3);
    loop_until_bit_is_clear(PIND,PD3);	/// these contradict available docs about ps/2
    loop_until_bit_is_set(PIND,PD3);	/// or I am missing sth
    for (i=8;i>0;i--) {
	loop_until_bit_is_clear(PIND,PD3);
	if (sc & 1) {
	    sbi(PORTD,PD4);
	    ++parity;
	} else {
	    cbi(PORTD,PD4);
	}
	sc >>= 1;
	loop_until_bit_is_set(PIND,PD3);
    }
    /* send parity */
    loop_until_bit_is_clear(PIND,PD3);
    if (parity & 0x01)
	cbi(PORTD,PD4);
    else
	sbi(PORTD,PD4);
    loop_until_bit_is_set(PIND,PD3);
    /* send stop bit */
    loop_until_bit_is_clear(PIND,PD3);
    sbi(PORTD,PD4);	//data high
    sbi(PORTD,PD3);	//clock pullup
    cbi(DDRD,PD4);	//release data
    loop_until_bit_is_set(PIND,PD3);
    /* ignore ACK */
    loop_until_bit_is_clear(PIND,PD3);
    loop_until_bit_is_set(PIND,PD3);
    /* reenable input IRQ */
    sbi(GIMSK, INT1);
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
    outp(0xfe, JOYPORT);	/// we're fucked
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
	outp(0xff, JOYPORT);	/// we're fucked
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
	    delay(50000);
	    ps2_sendackcmd(MSE_ReadData);
	    /* get data packet */
	    state = ps2_getbyte();
	    deltax= ps2_getbyte();
	    deltay= ps2_getbyte();
	    /* clear everything */
	    outp(0, JOYPORT);
	    /* button state - each of 3 buttons is fire */
	    if (state & 0x07)
		sbi(JOYPORT,JOYFIRE);
	    else
		cbi(JOYPORT,JOYFIRE);
	    /* now each of 4 directions */
	    if (deltax>XTHRESHOLD) {
		if (state & 0x10)
		    sbi(JOYPORT,JOYLEFT);
		else
		    sbi(JOYPORT,JOYRIGHT);
	    }
	    if (deltay>YTHRESHOLD) {
		if (state & 0x20)
		    sbi(JOYPORT,JOYUP);
		else
		    sbi(JOYPORT,JOYDOWN);
	    }
	}
    } else {
	// RMB is released - enter 1351 emulation mode
	outp(0xFA, JOYPORT);
	while (TRUE);
    }
}
