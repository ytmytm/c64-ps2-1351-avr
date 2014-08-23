
#include <inttypes.h>
#include <sig-avr.h>
#include <interrupt.h>
#include <io.h>
#include <stdlib.h>

#define FALSE  0
#define TRUE   (~FALSE)

int16_t	posx, posy;	/* X, Y displacement */

/**************************************/

void blink2(void) {
    if (bit_is_set(PORTA,PA2)) cbi(PORTA,PA2); else sbi(PORTA,PA2);
}
void blink1(void) {
    if (bit_is_set(PORTA,PA1)) cbi(PORTA,PA1); else sbi(PORTA,PA1);
}
void blink(void) {
    if (bit_is_set(PORTA,PA0)) cbi(PORTA,PA0); else sbi(PORTA,PA0);
}

/**************************************/

volatile uint8_t smalldelaydone;

/* this is for delays of up to 255usec, uses timer 0 */
void small_delay (uint8_t time) {
    /* delay for time usec+x (where x<=8) */
    outp(0x02, TCCR0);			// prescale by 8
    outp(256-time, TCNT0);		// setup time
    sbi(TIMSK, TOIE0);			// enable irq
    smalldelaydone = 0;			// clear flag
    while (smalldelaydone == 0);	// busy wait
}

INTERRUPT (SIG_OVERFLOW0) {
    outp(0, TCCR0);		// stop the counter
    cbi(TIMSK, TOIE0);		// disable irq from Output Compare A
    smalldelaydone = 0xff;	// clear flag
}

/* COMP1A - disable both potxy or one and callmeagain */
/* COMP1B - end of cycle */

/* these are precalculated */
volatile uint8_t sid_phase;		/* flag */
volatile uint8_t pot_t1l, pot_t2l;  /* timer values for first and last pot */
volatile uint8_t pot_t1h, pot_t2h;
volatile uint8_t potsmalldiff, potdiff;	  /* flag, difference between pots */
volatile uint8_t pot1, pot2;	  /* values to be written to ports */

#define POTPORT  PORTD
#define POTPIN	 PIND
#define POTDDR	 DDRD
#define POTXOFF  0x04
#define POTYOFF  0x02
#define POTXYOFF 0x06

INTERRUPT (SIG_INTERRUPT0) {
    cbi(GIMSK, INT0);	// disable INT0 irq
    /* pull POTX/Y down */
    outp(0x02, POTDDR);	// pull POTY pin down
//    outb(0x00, POTPORT);
    /* stop timer1 */
    outp(0x0a, TCCR1B);
//    outp(480 >> 8, OCR1AH);
//    outp(480 & 0xff, OCR1AL);
//    sbi(TIMSK, OCIE1A);
//    outb(pot_t1h, OCR1AH);
//    outb(pot_t1l, OCR1AL);
//    sbi(TIMSK, OCIE1A); // enable irq for Output Compare A
//    sid_phase = 0;	// first phase
    outp(480 >> 8, OCR1BH);	// setup timer for end of phase
    outp(480 & 0xff,OCR1BL);
    sbi(TIMSK, OCIE1B);	// enable irq for Output Compare B
//    outp(0x0a, TCCR1B);	// prescale by 8, start!
    blink();
}

INTERRUPT (SIG_OUTPUT_COMPARE1B) {
    // end of times
    outp(0x00,POTDDR);	// tristate POTX/Y
    outp(0x00,POTPORT);
    cbi(TIMSK, OCIE1B);	// disable irq from OCB
    sbi(GIMSK, INT0);   // enable irq from int0 (start of cycle)
//    outp(0x00, TCCR1B);	// stop timer1
//    sid_phase = 0xff;
    blink1();
    // end of cycle
}

int main(void) {

    // prepare whistles
    outp(0xff, DDRA);
    outp(0x00, PORTA);
    // setup POT as tristate (and INT0=POTX as input)
    outp(0x00, POTPORT);
    outp(0x00, POTDDR);
    // enable irqs
    sbi(MCUCR, ISC01); cbi(MCUCR, ISC00); // int0 on falling
//    cbi(MCUCR, ISC01); cbi(MCUCR, ISC00); // int0 on low level
    sbi(GIMSK, INT0);			  // enable int0
    // setup/enable irq from int0
    sei();
    while (TRUE) {
	small_delay(200);
	blink2();
    }
}
