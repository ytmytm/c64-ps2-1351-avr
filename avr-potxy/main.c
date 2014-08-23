
/*
    algorytm: znany
    po opuszczeniu na int0 przeslac dane (interesuja mnie tylko opoznienia
    pytanie: czy pin z int0 ma byc zawsze wejsciem a jakis inny z nim zwarty
	zawsze wyjsciem? czy tez sterowac wszystkim z int0? (na razie to pierwsze)
*/
// TODO: busyloop w COMPA
/* dziala dla x>>y, sprawdzic dla x<<y, x~y, x=y, cos wymyslec dla malych opoznien */
/* kazdy IRQ jest o 8-10usec za pozno, brac to pod uwage przy ustawianiu zegara */
/* uaktualninie (int0primer) tylko w bezpiecznej fazie - gdy sidphase==0xff
   (jesli tam nie ma przerwy, to przeniesc sidphase=0xff do COMPA# - obok
   wylaczenia przerwan z COMPA

// obliczanie polozenia; mysz raportuje *przesuniecie*, nalezy przechowywac
   *polozenie*, do obliczen trzeba wziac mlodsze 5 bitow polozenia (0-63),
   *polozenie* nie moze byc ujemne
*/

/*
    - sense start of SID measuring cycle
    - spend exactly 256+64 clocks (usec)
    - spend X*2 until raising POTX, spend Y*2 until raising POTY
    - spend remaining cycles until #480 from start
    - drop both POTX,POTY (tristate them)
*/

/*  - INT0# SID POTX opuszczony - podlaczony do INT0 generuje przerwanie
    - przerwanie ustawia opoznienie na koniec cyklu (#480) i na wylaczenie
      pierwszej linii
    - COMPA# pierwsza linia zostaje wylaczona po odczekaniu #256+64+#X
    - jesli roznica jest 'mala', to czeka w busyloop i wylacza druga
      w przeciwnym wypadku laduje do COMPA czas drugiego opoznienia
    - COMPA# druga linia zostaje wylaczona, nie ma wiecej przerwan z COMPA
    - COMPB# koniec cyklu, tristate POTs
*/

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
volatile uint8_t pot_t1l, pot_t2l, pot_t3l;  /* timer values for first and last pot */
volatile uint8_t pot_t1h, pot_t2h, pot_t3h;
volatile uint8_t potsmalldiff, potdiff;	  /* flag, difference between pots */
volatile uint8_t pot1, pot2;	  /* values to be written to ports */

#define POTPORT  PORTD
#define POTPIN	 PIND
#define POTDDR	 DDRD
#define POTXOFF  0x04
#define POTYOFF  0x02
#define POTXYOFF 0x06

//INTERRUPT (SIG_INTERRUPT0) {
void int0(void) {
    cbi(GIMSK, INT0);	// disable INT0 irq
    /* pull POTX/Y down */
    outp(0xff, POTDDR);	// pull both POTX/Y pins down ??? changing DDR necessary?
//    outb(0x00, POTPORT);
    /* stop timer1 */
//    outb(0x00, TCCR1B);
    outp(0x0a, TCCR1B);
    outp(pot_t1h, OCR1AH);
    outp(pot_t1l, OCR1AL);
//    outb(0x02, TCCR1B);	// prescale by 8, start!
    sbi(TIMSK, OCIE1A); // enable irq for Output Compare A
    sid_phase = 0;	// first phase
//    outb(480 >> 8, OCR1BH);	// setup timer for end of phase
//    outb(480 & 0xff,OCR1BL);
//    sbi(TIMSK, OCIE1B);	// enable irq for Output Compare B
//    blink();
    sbi(PORTA,PA0);
}

INTERRUPT (SIG_OUTPUT_COMPARE1A) {
    switch (sid_phase) {
    case 0:
	outp(pot1, POTPORT);
	outp(pot_t2h, OCR1AH);
	outp(pot_t2l, OCR1AL);
	++sid_phase;
	blink1();
	break;
    case 1:
	outp(POTXYOFF, POTPORT);
	outp(pot_t3h, OCR1AH);
	outp(pot_t3l, OCR1AL);
	++sid_phase;
	blink1();
	break;
    case 2:
	outp(0x00,POTDDR);	// tristate POTX/Y
	outp(0x00,POTPORT);
	cbi(TIMSK, OCIE1A);	// disable irq from OCB
//		sbi(GIMSK, INT0);   // enable irq from int0 (start of cycle)
	outp(0x00, TCCR1B);	// stop timer1
	sid_phase = 0xff;
//    		blink2();
// end of cycle
	cbi(PORTA,PA0);
	break;
    }
}


/*
INTERRUPT (SIG_OUTPUT_COMPARE1A) {
    if (sid_phase==0) {
	outb(pot1, POTPORT);	// raise first
	if (potsmalldiff) {
	    // wait potdiff usec
	    // ...XXX wait here... use timer0 or sth
	    // raise both
	    outb(POTXYOFF, POTPORT); // raise both
	    cbi(TIMSK, OCIE1A);	     // disable irq from OCA
	} else {
	    // reload Output Compare A with new value
	    ++sid_phase;
	    outb(pot_t2h, OCR1AH);
	    outb(pot_t2l, OCR1AL);
	    blink1();
	}
    } else {
	outb(POTXYOFF, POTPORT);	// raise both
	cbi(TIMSK, OCIE1A);		// disable irq from OCA
	blink1();
    }
}
*/
/*
INTERRUPT (SIG_OUTPUT_COMPARE1B) {
    // end of times
    outp(0x00,POTDDR);	// tristate POTX/Y
    outp(0x00,POTPORT);
    cbi(TIMSK, OCIE1B);	// disable irq from OCB
    sbi(GIMSK, INT0);   // enable irq from int0 (start of cycle)
    outp(0x00, TCCR1B);	// stop timer1
    sid_phase = 0xff;
//    blink2();
    // end of cycle
    cbi(PORTA,PA0);
}
*/

/* this prepares values for interrupt routines */
void int0primer(void) {
uint16_t potx, poty;
    potx=256+64+(2*posx);
    poty=256+64+(2*posy);
    if (potx < poty) {
	pot_t1l = potx & 0xff;
	pot_t1h = potx >> 8;
//	pot_t2l = poty & 0xff;
//	pot_t2h = poty >> 8;
	pot_t2l = (poty - potx) & 0xff;
	pot_t2h = (poty - potx) >> 8;
	pot_t3l = (480 - poty) & 0xff;
	pot_t3h = (480 - poty) >> 8;
	pot1 = POTXOFF;
    } else {
	pot_t3l = (480 - potx) & 0xff;
	pot_t3h = (480 - potx) >> 8;
	pot_t2l = (potx - poty) & 0xff;
	pot_t2h = (potx - poty) >> 8;
	pot_t1l = poty & 0xff;
	pot_t1h = poty >> 8;
	pot1 = POTYOFF;
    }
    potsmalldiff = 0;
    potdiff = abs(potx-poty);
    if (potdiff < 20)		// don't do 2nd irq if no time for that
	potsmalldiff = 1;
    if (potdiff == 0)
	pot1 = POTXYOFF;
}

int main(void) {
	posx=0;
	posy=31;

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
    blink();
    int0primer();
    blink();
    while (TRUE) {
	int0();
	while (sid_phase!=0xff);
	blink2();
    }
}
