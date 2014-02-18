/************************************************************************/
/*                      AVRTools                                        */
/*                      Beeper routines	using PWM						*/
/*                      Version 0.1                                     */
/* Code License: unknown (assume GNU GPL)								*/
/************************************************************************/
// Leo Meyer, 12/2009

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdint.h>
#include "beep.h"


//-----------------------------------------------------------------------------
// AlarmTon xxxx Hz
//
// Die Frequenz errechnet sich:
// 8Mhz(Taktfrequenz)/64(Prescaler)/2(Vollwelle)/1000hz(Ausgabefrequenz)= 62.5(Vergleichswert)
//-----------------------------------------------------------------------------
void sound_on(uint8_t ocr) {
    OCR1A = ocr;
    TCCR1A = 0;                                  // Toggle abschalten
    TCCR1B = 0;
    TCCR1A = (1<<COM1B0);                        // Toggle OC1B (PB2) on Compare Match.
    TCCR1B = (1<<CS10) | (1<<CS11) | (1<<WGM12); // /Prescaler: 64 laut und ok
    DDRB  |= 0b00000100;                         // PB2 als Ausgang
}

void sound_off() {
  {
    TCCR1A = 0;                 // Toggle abschalten
    DDRB &= ~0b00000100;        // PB2 als Eingang
  }
}



// sine table
const uint8_t sinetable[256] PROGMEM=
{	
	0x80,0x83,0x86,0x89,0x8c,0x8f,0x92,0x95,0x98,0x9c,0x9f,0xa2,0xa5,0xa8,0xab,0xae,
	0xb0,0xb3,0xb6,0xb9,0xbc,0xbf,0xc1,0xc4,0xc7,0xc9,0xcc,0xce,0xd1,0xd3,0xd5,0xd8,
	0xda,0xdc,0xde,0xe0,0xe2,0xe4,0xe6,0xe8,0xea,0xec,0xed,0xef,0xf0,0xf2,0xf3,0xf5,
	0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfc,0xfd,0xfe,0xfe,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xfe,0xfd,0xfc,0xfc,0xfb,0xfa,0xf9,0xf8,0xf7,
	0xf6,0xf5,0xf3,0xf2,0xf0,0xef,0xed,0xec,0xea,0xe8,0xe6,0xe4,0xe2,0xe0,0xde,0xdc,
	0xda,0xd8,0xd5,0xd3,0xd1,0xce,0xcc,0xc9,0xc7,0xc4,0xc1,0xbf,0xbc,0xb9,0xb6,0xb3,
	0xb0,0xae,0xab,0xa8,0xa5,0xa2,0x9f,0x9c,0x98,0x95,0x92,0x8f,0x8c,0x89,0x86,0x83,
	0x80,0x7c,0x79,0x76,0x73,0x70,0x6d,0x6a,0x67,0x63,0x60,0x5d,0x5a,0x57,0x54,0x51,
	0x4f,0x4c,0x49,0x46,0x43,0x40,0x3e,0x3b,0x38,0x36,0x33,0x31,0x2e,0x2c,0x2a,0x27,
	0x25,0x23,0x21,0x1f,0x1d,0x1b,0x19,0x17,0x15,0x13,0x12,0x10,0x0f,0x0d,0x0c,0x0a,
	0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x03,0x02,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x02,0x03,0x03,0x04,0x05,0x06,0x07,0x08,
	0x09,0x0a,0x0c,0x0d,0x0f,0x10,0x12,0x13,0x15,0x17,0x19,0x1b,0x1d,0x1f,0x21,0x23,
	0x25,0x27,0x2a,0x2c,0x2e,0x31,0x33,0x36,0x38,0x3b,0x3e,0x40,0x43,0x46,0x49,0x4c,
	0x4f,0x51,0x54,0x57,0x5a,0x5d,0x60,0x63,0x67,0x6a,0x6d,0x70,0x73,0x76,0x79,0x7c	
};

volatile uint16_t beep_scale = 523;
volatile uint16_t beep_count = 0;
volatile uint32_t beep_dur;

// interrupt service routine
#ifdef atmega8
ISR(TIMER1_COMPA_vect){
	OCR1A = pgm_read_byte(&sinetable[beep_count >> 8]);
   	beep_count += beep_scale;
	beep_dur--;
	if (beep_dur == 0) {
		// finished, switch off interrupt
		TIMSK &= ~(1 << OCIE1A);
		
		// stop PWM
		TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0) | (1 << WGM10));
		
		// stop the timer
		TCCR1B &= ~((1 << CS10) | (1 << CS11) | (1 << CS12));
	}
#elif atmega168
ISR(TIMER1_COMPB_vect){
	OCR1B = pgm_read_byte(&sinetable[beep_count >> 8]);
	OCR1A = pgm_read_byte(&sinetable[beep_count >> 8]);
   	beep_count += beep_scale;
	beep_dur--;
	if (0 && beep_dur == 0) {
		// finished, switch off interrupt
		TIMSK1 &= ~(1 << OCIE1B);
		
		// stop PWM
		TCCR1A &= ~((1 << COM1B0));

		TCCR1B &= ~ (1 << WGM10);
		
		// stop the timer
		TCCR1B &= ~((1 << CS10) | (1 << CS11) | (1 << CS12));
	}
#else
	#error "Unknown controller"
#endif
}

#define STEP_FACTOR	16
void tone(uint16_t freq, uint32_t duration) {
	// calculate scale from frequency
	long scale = freq * STEP_FACTOR;
	
	beep(scale, duration);
}

void beep(uint16_t scale, uint32_t duration) {

	beep_scale = scale;
	beep_count = 0;
	beep_dur = duration;
	
	cli();

    // initialize PWM
	DDRB |= 1 << OUTPUT_PIN;
	
#ifdef atmega8
	
	// PWM compare value (50% duty cycle)
	OCR1A = 0x80;
	
	// Output compare OC1A 8 bit non inverted PWM
	// Timer Counter Control Register!
	// Bit:   7	  6	 5	4     3     2     1     0
	// Bed: COM1A1 COM1A0 COM1B1 COM1B0 FOC1A FOC1B WGM11 WGM10
	// Hier:  1       0      0      1     0     0     0     1
	TCCR1A = (1 << COM1A1) | (1 << COM1A0) | (1 << WGM10);

	// start timer, no prescaler
	TCCR1B |= (1 << CS10);
	
	// Enable timer interrupt
	// Bit:   7	6      5      4     3      2     1      0
	// Bed: OCIE2 TOIE2 TICIE1 OCIE1A OCIE1B TOIE1 ------ TOIE0	
	// Hier:  0	0      0      1     0      0     0      0
	TIMSK |= (1 << OCIE1A);
	
#elif atmega168
	// PWM compare value (50% duty cycle)
	OCR1B = 0x80;
	OCR1A = 0x80;
	
	// Output compare OC1B 8 bit non inverted PWM
	TCCR1A = (1 << COM1B1)  | (1 << WGM10);

	// start timer, no prescaler
	TCCR1B |= (1 << CS10);
	
	// Enable timer interrupt
	TIMSK1 |= (1 << OCIE1B);
	
#else
	#error "Unknown controller"
#endif
	
	//enable global interrupts
	sei();
}


