/*
 * EE4524 Project2
 * This program takes inputs from the user via the serial port and outputs data from 
 * ADC sources 0-2.
 * Author: Charlie Gorey O Neill
 * ID:18222803
 */ 

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#define bitRead(value, bit) (((value) >> (bit)) & 0x01) 

void sendmsg (char *s);
void init_PORTS(void);
void init_timer0(void);
void init_timer2(void);
void init_adc(void);
void init_USART(void);

int adc_status = 2;
int ADC_flag;
int qcntr = 0;
int sndcntr = 0;				// indexes into the queue
unsigned char queue[50];        // character queue
char ch;						// character variable for received character
uint16_t adc_reading;			// to store ADC value
uint8_t readTemp;				// to store reading from LM35
uint16_t brightness;			// to store reading from LDR
uint8_t continuous_mode = 0;	// goes to 1 if user inputs 'c'
uint32_t millivolts;			// to store conversion from adc

/*unsigned long clocksHigh, clocksLow;
uint16_t timecount1;*/

void init_PORTS()
{
	DDRB = 0x08;			 //OCR2A pin made as output
	PINB = 0x00;			// initialize as zero
}

void init_timer0()
{
	TIMSK0 = 0;
	TCCR0A = 0x00;		// Leave reset states alone  
	TCNT0 =  0x06;		//256-6 = 250 & 250*64us = 16ms
	TCCR0B = (1<<CS01) | (1<<CS00);	// clkIO/1024 prescale
}

void init_timer2()
{	
	/*
	Initialize Timer2 for 8 bit Phase Correct mode, PWM mode, TOP = 0xFF
	Clear O2CA on compare match, set O2CA at BOTTOM */
	TIMSK2 = 0;
	TCCR2A = (1<<COM2A1) | (1<<WGM20);	
	TCCR2B = (1<<CS22);						// clkIO/256 prescale
}

void init_adc()
{
	/*
	AVCC set as VREF, ADC2 as initial input.
	ADC prescaler 128
	ADC Trigger = Timer/Counter0 overflow
	*/
	ADMUX = (1<<REFS0) | (2<<MUX0);	
	ADCSRA = (1<<ADEN) | (1<<ADATE) | (1<<ADIE) | (1<<ADPS0) | (1<<ADPS1) | (1<<ADPS2); 
	ADCSRB = (1<<ADTS2); 
}

void init_USART()
{
	/*
	U2XO = 1: Asynchronous double speed mode
	enable receiver, transmitter and transmit interrupt
	baud rate: 207 = 9600, 16 = 115200
	8 bit data 
	*/
	UCSR0A = (1<<U2X0);
	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<TXCIE0) | (0<<RXCIE0); 
	UBRR0 = 16; 
	UCSR0C = (3<<UCSZ00);; 
	
}


int main(void)
{
	
	char ch_recieved;		// character variable for received character
	uint16_t ocr2a_setting = 0; //Value to be read when OCR2A is to be reported
	qcntr = 0;				//queue counter for char in the strings
	sndcntr = 0;			//counter for char sent
	char msg_buffer[50];
	
	init_PORTS();
	init_timer0();
	init_timer2();
	init_USART();
	init_adc();

	sei(); /*global interrupt enable */

	while (1)
	{
		if (UCSR0A & (1<<RXC0)) /*check for character received*/
		{
			ch_recieved = UDR0;    /*get character sent from PC*/
			switch (ch_recieved)
			{
				//read commands first
				case 'A':
				case 'a':
					sprintf(msg_buffer, "ADC reading = %i", adc_reading); //Report the ADC conversion result.
					sendmsg(msg_buffer);
				break;
				case 'V':
				case 'v':
					sprintf(msg_buffer, "Voltage = %lu mV", millivolts); //Reports the adc reading in millivolts
					sendmsg(msg_buffer);
				break;
				case 'M':		//set adc status for LM35(ADC2) which will be checked in isr
				case 'm':
					adc_status = 2;
					sprintf(msg_buffer, "ADC2 selected");
					sendmsg(msg_buffer);
				break;
				case 'N':		//set adc status for LDR(ADC1) which will be checked in isr
				case 'n':
					adc_status = 1;
					sprintf(msg_buffer, "ADC1 selected");
					sendmsg(msg_buffer);
				break;
				case 'P':		//set adc status for potentiometer(ADC0) which will be checked in isr
				case 'p':
					adc_status = 0;
					sprintf(msg_buffer, "ADC0 selected");
					sendmsg(msg_buffer);
				break;
				case 'T':
				case 't':		
					
					if(adc_status == 2)
					{								//if ADC2 is selected report the temperature in deg Celcius
						readTemp = millivolts/10;	// conversion rate taken from LM35 datasheet
						sprintf(msg_buffer, "LM35 Temp = %i deg C", readTemp);
						sendmsg(msg_buffer);
					}
					else
					{								//if ADC2 is not selected show error
						sprintf(msg_buffer, "Choose ADC2");
						sendmsg(msg_buffer);
					}
					
				break;
				case 'l':
				case 'L':
				if(adc_status == 1){				//if ADC1 is selected report the brightness
					brightness = adc_reading;
					if (brightness>512){			// adc>512 = bright
						sprintf(msg_buffer, "It is bright");
						sendmsg(msg_buffer);
						}
					else{							// adc<512 = dark
						sprintf(msg_buffer, "It is dark");
						sendmsg(msg_buffer);
						}
					}
				else{								// if ADC1 is not selected show error
					sprintf(msg_buffer, "Choose ADC1");
					sendmsg(msg_buffer);
					}
				break;
				case 'S':
				case 's':							// report value of pwm setting
					sprintf(msg_buffer, "OCR2A value is %i",  ocr2a_setting);
					sendmsg(msg_buffer);
				break;
				case 'C':
				case 'c':
					continuous_mode = 1;
					sprintf(msg_buffer, "continous mode ON");
					sendmsg(msg_buffer);
				break;
				case 'E':
				case 'e':
					continuous_mode = 0;
					sprintf(msg_buffer, "continous mode OFF");
					sendmsg(msg_buffer);
				break;
				//For the PWM control I used the following: x% multiplied by 255
				case '0':
					OCR2A = 0;
					ocr2a_setting = 0;
				break;
				case '1':
					OCR2A = 25;
					ocr2a_setting = 25;
				break;
				case '2':
					OCR2A = 51;
					ocr2a_setting = 51;
				break;
				case '3':
					OCR2A = 77;
					ocr2a_setting = 77;
				break;
				case '4':
					OCR2A = 102;
					ocr2a_setting = 102;
				break;
				case '5':
					OCR2A = 128;
					ocr2a_setting = 128;
				break;
				case '6':
					OCR2A = 154;
					ocr2a_setting = 154; 
				break;
				case '7':
					OCR2A = 179;
					ocr2a_setting = 179;
				break;
				case '8':
					OCR2A = 205;
					ocr2a_setting = 205;
				break;
				case '9':
					OCR2A = 230;
					ocr2a_setting = 230;
				break;
			}
		}
/*Continous mode functionality handled here
Checks for new adc reading, continous mode is chosen and that USART data reg is empty
*/

		if (ADC_flag==1 && continuous_mode==1 && (bitRead(UCSR0A,5)))
		{	
			switch (adc_status)
			{
			case 2:
				readTemp = millivolts/10;
				sprintf(msg_buffer, "%i degrees C", readTemp);
				sendmsg(msg_buffer);
				break;
			case 1:
				if (brightness>512){
					sprintf(msg_buffer, "bright");
					sendmsg(msg_buffer);
				}
				else{
					sprintf(msg_buffer, "dark");
					sendmsg(msg_buffer);
				}
				break;
			case 0:
				millivolts = ((uint32_t)adc_reading*5000)/1023;
				sprintf(msg_buffer, "%lumV", millivolts);
				sendmsg(msg_buffer);
				break;
			}
		}					//end of adc flag if
			ADC_flag = 0;	//resets adc flag to get new reading
	}						//end of while
}							//end of main




/************************************************************************************/
/* USART sendmsg function															*/
/*this function loads the queue and													*/
/*starts the sending process														*/
/************************************************************************************/

void sendmsg (char *s)
{
	qcntr = 0;    /*preset indices*/
	sndcntr = 1;  /*set to one because first character already sent*/
	
	queue[qcntr++] = 0x0d;   /*put CRLF into the queue first*/
	queue[qcntr++] = 0x0a;
	while (*s)
		queue[qcntr++] = *s++;   /*put characters into queue*/
		
	UDR0 = queue[0];  /*send first character to start process*/
}

/********************************************************************************/
/* Interrupt Service Routines													*/
/********************************************************************************/

/*this interrupt occurs whenever the */
/*USART has completed sending a character*/

ISR(USART_TX_vect)
{
	/*send next character and increment index*/
	if (qcntr != sndcntr)
		UDR0 = queue[sndcntr++];
}

ISR (ADC_vect)//handles ADC interrupts
{
ADC_flag = 1; 
adc_reading = ADC;
millivolts = ((uint32_t)adc_reading*5000)/1023; /* put this in the isr because Temp = 0 
										if I did not ask for mV first as it is initialised to 0*/

	if(adc_status==2)
	{
		ADMUX = 0x42; //ADMUX reg with bits REFS0 & MUX2 set to 1
	}
	else if(adc_status==1)
	{
		ADMUX = 0x41; //ADMUX reg with bits REFS0 & MUX1 set to 1
	}
	else
	{
		ADMUX = 0x40; //ADMUX reg with bits REFS0 set to 1
	}
TIFR0 |= (1<<TOV0);   // reset OVF bit
}

	


