#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>

#define HIV	_BV(5)	//B -> Transistor base
#define VCC	_BV(4)	//B -> Target pin 8 Vcc
#define SDO _BV(3)	//B -> Target pin 7

#define SCI _BV(0)	//D -> Target pin 2
#define SII _BV(1)	//D -> Target pin 6
#define SDI _BV(2)	//D -> Target pin 5



#define LFUSE 0xe2
#define HFUSE 0xdd
#define EFUSE 0x01

#define SetBit(var,pos,val) if(val) var |= _BV(pos); else var &= ~_BV(pos)
#define GetBit(var,pos) ((var) & (1<<(pos)))

#define SCI_PULSE	_delay_us(1); PORTD |= SCI; _delay_us(1); PORTD &= ~SCI


inline uint8_t HVSPBit(uint8_t instrBit, uint8_t dataBit) {
	SetBit(PORTD, SII, instrBit);
	SetBit(PORTD, SDI, dataBit);
	SCI_PULSE;
	return GetBit(PINB, SDO)
}

static void HVSPTransfer(uint8_t instrIn, uint8_t dataIn) {
	//
	// First bit, data out only
	//
	uint8_t dataOut = HVSPBit(0, 0);

	//
	// Next bits, data in/out
	//
	uint8_t i = 0;
	for (i = 0; i < 7; ++i) {
		HVSPBit((instrIn & 0x80) != 0, (dataIn & 0x80) != 0);
		instrIn <<= 1;
		dataIn  <<= 1;
	}
	//
	// Last data out bit
	//
	HVSPBit(instrIn & 0x80, dataIn & 0x80);

	//
	// Two stop bits
	//
	HVSPBit(0, 0);
	HVSPBit(0, 0);
}

static void ProgramFuseLock(uint8_t d0, uint8_t value, uint8_t i2, uint8_t i3)
{
	HVSPTransfer(0x4C, d0);
	HVSPTransfer(0x2C, value);
	HVSPTransfer(i2, 0x00);
	HVSPTransfer(i3, 0x00);
	_delay_ms(50);
}

void all_low(){
	DDRB  |= (VCC|HIV|SDO);
	PORTB &= ~(VCC|SDO);// Vcc and data out off
	PORTB |= HIV;		// 12v off
	DDRD  |= (SCI|SDI|SII);
	PORTD &= ~(SCI|SDI|SII);
	_delay_ms(50);
}

void init_hvsp() {
	PORTB |= VCC;		// Vcc on
	_delay_us(80);
	PORTB &= ~HIV;		// turn on 12v
	_delay_us(15);
	DDRB  &= ~SDO;		// release SDO
	_delay_us(300);
}

void program_fuse() {

	ProgramFuseLock(0x40, LFUSE, 0x64, 0x6C);
	ProgramFuseLock(0x40, HFUSE, 0x74, 0x7C);
	ProgramFuseLock(0x40, EFUSE, 0x66, 0x6E);
}

void reset_fuse() {
	// enter hv mode, everything go low
	all_low();
	init_hvsp();

	//reset fuse
	program_fuse();

	// done, turn things off
	all_low();
}

void main(void) {
	DDRB = PORTB = 0;
	DDRD = PORTD = 0;
	DDRB   = (VCC|HIV|SDO);
	PORTB  = HIV;		// 12v off
	_delay_ms(100);

	reset_fuse();

	while (1) { }
}

