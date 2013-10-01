#define F_CPU 8000000UL

#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define BUTTON_DDR	 	DDRD
#define BUTTON_PORT 	PORTD
#define BUTTON_PIN 		_BV(4)

#define ST_HOLD		0x80
#define ST_PRESSED	0x40
#define ST_BUTTON   (ST_HOLD|ST_PRESSED)
#define ST_TICKED	0x20
#define ST_12HR 	0x10
#define ST_REFRESH	0x08
#define ST_BUZZ     0x04
#define ST_SETUP   	0x03

// ticks per second and devired values
#define TPS   (F_CPU/256)
#define TPS_2 (TPS/2)
#define TPS_4 (TPS/4)

#define LONG_HOLD (TPS/3)

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

uint8_t chip_sig[] = { 0xee, 0xee, 0xee, 0xee, 0x00, 0x00 };

void read_chip(uint8_t release_reset) {
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
	uint8_t *pdata = chip_sig;
	uint8_t i;
	
	//read device signature
	for (i=0;i<3;i++) {
		cmd[2] = i;
		if (i)
			*pdata++ = hv_cmd(&cmd[2], 3);
		else
			*pdata++ = hv_cmd(cmd, 4);
	}

	uint8_t  id[] = { 0x07, 0x18, 0x26, 0x3b, 0x1b, 0x27, 0x3c, };
	uint8_t mcu[] = { 0x13, 0x25, 0x45, 0x85, 0x24, 0x44, 0x84, };

	if ((chip_sig[0] == 0x1e) && ((chip_sig[1]&0xf0) == 0x90) &&
		!(chip_sig[2]&0xf0)) {
		chip_sig[1] <<= 4;
		chip_sig[1] |= chip_sig[2];
		
		for (i=0;i<7;i++) {
			if (chip_sig[1] == id[i]) {
				chip_sig[0] = i;
				chip_sig[1] = mcu[i];
			}
		}
		pdata--;
	}

	//reset fuse
	if (release_reset) {
		cmd[0] = 0x40; 
		// write fuse low bits
		cmd[3] = 0x2c; cmd[5] = 0x64; 
		if (chip_sig[1] == 0x13 || chip_sig[1] == 0x44 || chip_sig[1] == 0x84)
			cmd[2] = 0x6a; 
		else
			cmd[2] = 0x62; 
		hv_cmd(cmd, 4); _delay_ms(50);
		// write fuse high bits
		cmd[5] = 0x74; cmd[7] = 0x7c;
		if (chip_sig[1] == 0x13)
			cmd[2] = 0xff; 
		else
			cmd[2] = 0xdf; 
		hv_cmd(cmd, 4); _delay_ms(50);
		// write fuse extended bits
		if (chip_sig[1] != 0x13) {
			cmd[5] = 0x66; cmd[7] = 0x6e;
			cmd[2] = 0x01; 
			hv_cmd(cmd, 4); _delay_ms(50);
		}
	}

	// read fuse
	cmd[0] = 0x04; cmd[2] = 0x00;

	cmd[3] = 0x68; cmd[5] = 0x6c; 		// fuse low
	*pdata++ = hv_cmd(cmd, 3);

	cmd[3] = 0x7a; cmd[5] = 0x7c; 		// fuse high
	*pdata++ = hv_cmd(cmd, 3);

	if (chip_sig[1] != 0x13) {
		cmd[3] = 0x6a; cmd[5] = 0x6e; 	// fuse extended
		*pdata++ = hv_cmd(cmd, 3);
	}

	// done, turn things off
	PORTB |= HIV;
	PORTB &= ~(VCC|SDO);
	PORTD &= ~(SCI|SDI|SII);
}

void main(void) {

	TCCR0B |= 0x01;
	TIMSK  |= _BV(TOIE0);
	TCNT0   = 0x00;

	DDRA = PORTA = 0;
	DDRD = PORTD = 0;
	DDRB   = (VCC|HIV|SDO);
	PORTB  = HIV;		// 12v off
	_delay_ms(50);
	read_chip(0);


	read_chip(1);
	_delay_us(100);
	read_chip(0);
	
	while (1) { }
}

