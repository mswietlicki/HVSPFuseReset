#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>

#define HIV	_BV(7)	
#define VCC	_BV(6)	
#define SDO _BV(0)

#define SCI _BV(0)
#define SII _BV(5)
#define SDI _BV(6)

#define SCI_PULSE	_delay_us(1); PORTD |= SCI; _delay_us(1); PORTD &= ~SCI;

uint8_t hv_cmd(uint8_t *dptr, uint8_t cnt) {
	// data format is like write 0_DDDD_DDDD_00
	//                      read D_DDDD_DDDx_xx
	uint8_t sdo=0x00;
	while (cnt) {
		uint8_t sdi = *dptr++;
		uint8_t sii = *dptr++;
		uint8_t cmp=0x80;

		sdo = 0x00;
		PORTD &= ~(SDI|SII);
		SCI_PULSE;

		// 0x1e92 06
		// 0x62df	b0110 0010 1101 1111
		while (cmp) {
			sdo <<= 1;
			if (PINB&SDO) sdo |= 0x01;
			PORTD &= ~(SDI|SII);
			if (cmp&sdi) PORTD |= SDI;
			if (cmp&sii) PORTD |= SII;
			SCI_PULSE;
			cmp >>= 1;
		}

		PORTD &= ~(SDI|SII);
		SCI_PULSE;
		SCI_PULSE;
		_delay_us(100);
		cnt--;
	}

	return sdo;
}

void reset_fuse() {
	// enter hv mode, everything go low
	DDRB  |= (VCC|HIV|SDO);
	PORTB &= ~(VCC|SDO);// Vcc and data out off
	PORTB |= HIV;		// 12v off
	DDRD  |= (SCI|SDI|SII);
	PORTD &= ~(SCI|SDI|SII);
	//_delay_ms(50);

	PORTB |= VCC;		// Vcc on
	_delay_us(40);
	PORTB &= ~HIV;		// turn on 12v
	_delay_us(15);
	DDRB  &= ~SDO;		// release SDO
	_delay_us(300);

	uint8_t cmd[] = { 0x08, 0x4c, 0x00, 0x0c, 0x00, 0x68, 0x00, 0x6c, };

	//reset fuse
	cmd[0] = 0x40; 
	// write fuse low bits
	cmd[3] = 0x2c; cmd[5] = 0x64; 
	cmd[2] = 0xe2; 					//lfuse value
	hv_cmd(cmd, 4); 
	_delay_ms(50);
	// write fuse high bits
	cmd[5] = 0x74; cmd[7] = 0x7c;
	cmd[2] = 0xdf; 					//hfuse value
	hv_cmd(cmd, 4); 
	_delay_ms(50);
	// write fuse extended bits
	cmd[5] = 0x66; cmd[7] = 0x6e;
	cmd[2] = 0x01; 					//efuse value
	hv_cmd(cmd, 4); 
	_delay_ms(50);
	// done, turn things off
	PORTB |= HIV;
	PORTB &= ~(VCC|SDO);
	PORTD &= ~(SCI|SDI|SII);
}

void main(void) {
	DDRA = PORTA = 0;
	DDRD = PORTD = 0;
	DDRB   = (VCC|HIV|SDO);
	PORTB  = HIV;		// 12v off
	_delay_ms(100);

	reset_fuse();
	
	while (1) { }
}

