/*
 * main.c
 *
 * Created: 2/24/2023 10:26:02 PM
 *  Author: JimAndVal
 */ 

#include <xc.h>
#include <avr/io.h>
#include <avr/interrupt.h>

unsigned short startingCount;
unsigned char numSums;
signed long sumPulseLength;
volatile unsigned short avgPulseLength;
int temp;

void process_commands();

int main(void)
{
	
	
   //These fuses are configured in Device Programming 
   //Full speed clock
   //protected_write_io((void *)&CLKPR, 1 << CLKPCE, (0 << CLKPS3) | (0 << CLKPS2) | (0 << CLKPS1) | (0 << CLKPS0));
   
   //Turn off the SPI port, the TWI port, and the ADC
   PRR = (1 << PRSPI) | (0 << PRTIM2) | (0 << PRTIM0) | (
   0 << PRTIM1) | (1 << PRTWI) | (0 << PRUSART0) | (1 << PRADC);
   
   //Set Port B pin 3 (OC2A) as output (PWN distance), all others as inputs
   DDRB = 0x08;
   
   //Enable all Port B pull ups
   PORTB = 0xFF;
   
   //Set Port C pins as inputs
   DDRC = 0x00;
   
   //Enable all Port C pullups
   PORTC = 0xC0;
   
   //Set Port D pin 6 (OC0A) as an output (trigger to transducer), all others inputs
   DDRD = 0x40;
   
   //Init Port D output pin 6 (OC0A) to zero and enable pullups on inputs
   PORTD = ~0x40;
   
   //Set USART baud rate to 38,400
   UBRR0H = 0x00;
   UBRR0L = 25;
   
   //No double speed
   UCSR0A = 0x00;
   
   //Enable the receiver and transmitter
   UCSR0B = (1<<RXEN0)|(1<<TXEN0);
   
   // Set frame format: 8data, 2stop bit
   UCSR0C = (1<<USBS0) |(3<<UCSZ00);
   
   //Configure Timer/Counter 0 for normal operation, toggle OC0A
   TCCR0A = 1 << COM0A0;

// Divide by 1024 prescaler
   TCCR0B =  (1 << CS02) |(1 << CS00);
   
 //Configure Timer/Counter 1 for normal operation, no output   
   TCCR1A = 0x00;

//Capture on rising edge, divide by 64 prescaler
   TCCR1B = (1 << ICES1) | (1 << CS11) | (1 << CS10);
   
//Configure Timer2 for fast PWM
   TCCR2A = (1 << COM2A1) | (1 << WGM21) | (1 << WGM20);
   
// No prescaler - full speed
   TCCR2B = (1 << CS20);
   
//Initiate to 50%
	OCR2A = 0x10;
   
//Enable input capture interrupt
   TIMSK1 = (1 << ICIE1);
   
//Initiate variables
	startingCount = 0;
	sumPulseLength = 0;
	numSums = 0;
   
   sei();
   
   while(1) {
		process_commands();
	}
}


ISR(TIMER1_CAPT_vect) {
	unsigned short endingCount;
	signed long pulseLength;
	unsigned short pwmPulse;
	
	//Was this a rising edge
	if (TCCR1B & (1 << ICES1)) {
		
		//Just save it and then change the edge select to look for the 
		//falling edge
		startingCount = ICR1L;
		startingCount += ((unsigned short)ICR1H) << 8;
		TCCR1B &= ~(1 << ICES1);
	
	//No, a falling edge
	} else {
		endingCount = ICR1L;
		endingCount += ((unsigned short)ICR1H) << 8;
		TCCR1B |= 1 << ICES1;
		
		pulseLength = (signed long)endingCount - (signed long)startingCount;
		if (pulseLength < 0) pulseLength += 65536;
		
		sumPulseLength += pulseLength;
		++numSums;
		if (numSums >= 16) {
			
			//Divide sum by 16 and the reset the accumulator and the sum counter
			avgPulseLength = sumPulseLength >> 4;
			sumPulseLength = 0;
			numSums = 0;
			
			//Limit result to 1023 ticks (about 27 inches) so as not to overflow the PWM
			if (avgPulseLength < 1024) {
				pwmPulse = avgPulseLength;
			} else {
				pwmPulse = 1023;
			}
			
			//Set the PWM output high time. Averaged output voltage is ~5/27 volts/inch
			OCR2A = pwmPulse >> 2;
		}
		
		
		
	}
	
	
	
}