/*
 * Matrix8x8Driver.c
 *
 * Created: 24.04.2013 17:25:27
 *  Author: marc
 */ 


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h> 

#define byte unsigned char

#define B_PLD_SCLK		PB0
#define B_COM_SER		PB1
#define B_PLD_VPRG		PB2
#define A_SHREG_SRCLK	PA0
#define A_PLD_XLAT		PA1
#define A_PLD_GSCLK		PA2
#define A_PLD_BLANK		PA3

const byte LED_COLS[] PROGMEM  = { 4, 1, 5, 2, 6, 7, 3, 0 };
const byte LED_ROWS[] PROGMEM  = { 8, 9, 10, 0, 1, 11, 2, 3, 12, 4, 5, 13, 6, 7, 14, 15 };

byte screen[] = { 
  030, 000, 000, 007, 007, 000, 000, 003,
  000, 000, 000, 007, 007, 000, 000, 000,
  000, 000, 000, 007, 007, 000, 000, 000,
  070, 070, 070, 077, 077, 070, 070, 070,
  070, 070, 070, 077, 077, 070, 070, 070,
  000, 000, 000, 007, 007, 000, 000, 000,
  000, 000, 000, 007, 007, 000, 000, 000,
  000, 000, 000, 007, 007, 000, 000, 033
};

//---------------------------------------------------------------- SPI --------------------------------------------------

unsigned char spi_state = 0;
unsigned char address;

ISR (USI_OVF_vect) { // SPI_STC_vect

	PORTA |= 1<<7;

	unsigned char c = USIBR;  // grab byte from SPI Buffer Register
	switch(spi_state) {
	case 0:
		if(c==0xA5) spi_state = 1;
		break;
	case 1:
		address = c & 0x3F; // screen max size
		spi_state = 2;
		break;
	case 2:
		screen[address] = c;
		spi_state = 0;
		break;
	}

	PORTA &= ~(1<<7);

}

//-------------------------------------------------------- Matrix Driver ------------------------------------------------------

void writeGS(unsigned int rows) {

	PORTB &= ~(1<<B_COM_SER); // digitalWrite(COM_SER, LOW);
	PORTB |= 1<<B_PLD_SCLK; // digitalWrite(PLD_SCLK, HIGH);
	PORTB &= ~(1<<B_PLD_SCLK); // digitalWrite(PLD_SCLK, LOW);

	int row_mask = 0x1;
	for(byte n=0;n<16;n++) {
		for(byte b=0;b<12;b++) {
						
			// digitalWrite(COM_SER, rows & row_mask ? HIGH: LOW);
			if(rows & row_mask) PORTB |= 1<<B_COM_SER;
			else PORTB &= ~(1<<B_COM_SER);
			
			PORTB |= 1<<B_PLD_SCLK; // digitalWrite(PLD_SCLK, HIGH);
			PORTB &= ~(1<<B_PLD_SCLK); // digitalWrite(PLD_SCLK, LOW);
		}
		row_mask<<=1;
	}
	
	PORTA |= 1<<A_PLD_BLANK; // digitalWrite(PLD_BLANK, HIGH);
	PORTA |= 1<<A_PLD_XLAT; // digitalWrite(PLD_XLAT, HIGH);
	PORTA &= ~(1<<A_PLD_XLAT); // digitalWrite(PLD_XLAT, LOW);
	PORTA &= ~(1<<A_PLD_BLANK); // digitalWrite(PLD_BLANK, LOW);
}

void shiftCol(byte v) {
	// digitalWrite(COM_SER, v ? LOW : HIGH);
	if(v) PORTB &= ~(1<<B_COM_SER);
	else PORTB |= 1<<B_COM_SER;
	PORTA |= 1<<A_SHREG_SRCLK; // digitalWrite(SHREG_SRCLK, HIGH);
	PORTA &= ~(1<<A_SHREG_SRCLK); // digitalWrite(SHREG_SRCLK, LOW);
}

void update() {
	static byte y = 8;

	PORTA |= 1<<A_PLD_BLANK; // digitalWrite(PLD_BLANK, HIGH);
	PORTA &= ~(1<<A_PLD_BLANK); // digitalWrite(PLD_BLANK, LOW);

	y--;
	shiftCol(y==0);

	byte col_index = pgm_read_byte_near(LED_COLS + y);
	PORTB |= 1<<B_PLD_VPRG; // digitalWrite(PLD_VPRG, HIGH);
	byte offset = col_index<<3;
	for(byte n=0;n<16;n++) {
		byte row_index = pgm_read_byte_near(LED_ROWS + n);
		byte c = screen[offset|row_index>>1];
		if(row_index&1) c = c & 07;
		else c>>=3;
		byte v = c<<3;
		for(byte b=0;b<6;b++) {
			// digitalWrite(COM_SER, v & 0x20 ? HIGH : LOW);
			if(v & 0x20) PORTB |= 1<<B_COM_SER;
			else PORTB &= ~(1<<B_COM_SER);
			PORTB |= 1<<B_PLD_SCLK; // digitalWrite(PLD_SCLK, HIGH);
			PORTB &= ~(1<<B_PLD_SCLK); // digitalWrite(PLD_SCLK, LOW);
			v<<=1;
		}
	}
	
	PORTA |= 1<<A_PLD_XLAT; // digitalWrite(PLD_XLAT, HIGH);
	PORTA &= ~(1<<A_PLD_XLAT); // digitalWrite(PLD_XLAT, LOW);
	PORTB &= ~(1<<B_PLD_VPRG); // digitalWrite(PLD_VPRG, LOW);

	if(y==0) y = 8;

	PORTA |= 1<<A_PLD_GSCLK; // digitalWrite(PLD_GSCLK, HIGH);
	PORTA &= ~(1<<A_PLD_GSCLK); // digitalWrite(PLD_GSCLK, LOW);
}

int main(void) {

	PORTA &= 0b11010000;
	PORTB &= 0b11111000;
			
	DDRB = 0b00000111; // PB0-PB2
	DDRA = 0b10101111; // PA7, MISO, PA0-PA3

	writeGS(0xFFFF);
	for(int i=0;i<8;i++) shiftCol(0);
	
	USICR = 1<<USIOIE | 1<<USIWM0 | 1<<USICS1; // OverFlow Interrupt, 3 Wire mode, Clock External positive Edge

	sei();
	
    while(1) {
	  update();
	  for(int i=0;i<500;i++); // delayMicroseconds(500);
    }
	
}