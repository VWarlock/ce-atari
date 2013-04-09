#include "lpc13xx.h"

#include <stdio.h>
#include "defs.h"

void init_timer32(void);
void setupClock(void);
void printit(void);
void appendBit(BYTE bit);

WORD times[2048];
WORD cnt;

BYTE newByte;
BYTE newBits;
BYTE prevBit;

DWORD sync;
BYTE scnt;

int main(void) {
	setupClock();

	printf("FDD FW\n");

	//	// config PIO2 outputs
	//	WORD *pio2dir = (WORD *) 0x50028000;
	//	*pio2dir = INDEX | TRACK0 | RDATA | WR_PROTECT | DISK_CHANGE;
	//
	//	// config PIO3 outputs
	//	WORD *pio3dir = (WORD *) 0x50038000;
	//	*pio3dir = ATN;

	// GPIO3IS  is 0 after reset = edge sensitive
	// GPIO3IEV is 0 after reset = interrupt on falling edge
	//	WORD *GPIO3RIS = (WORD *) 0x50038014;		// GPIO raw interrupt status register -- read to get interrupt status
	//	WORD *GPIO3IC  = (WORD *) 0x5003801C;		// GPIO interrupt clear register      -- write 1 to clear interrupt bit

	//----------
	// need to enable clock of IOCON to make IOCON work
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<16);			// enable IOCON clock

	init_timer32();

	cnt = 0;

//	// PIO1_0 - PIO1_2  --- A0 - A2
//	WORD *pio15  = (WORD *) 0x50010080;
//	WORD prev = 0;
//
//	while(1) {
//		WORD val;
//		val = *pio15;
//
//		if(val == prev) {	// no change? continue
//			continue;
//		}
//		prev = val;
//
////		if(val != 0) {		// rising edge? continue
////			continue;
////		}
//
//		DWORD dw = LPC_TMR32B0->TC;
//		LPC_TMR32B0->TC = 0;
//		if(dw > 0xffff) {
//			val = 0xffff;
//		} else {
//			val = dw;
//		}
//
//		if(cnt < 2048) {			// if don't have 2048 values
//			times[cnt] = val;		// store
//			cnt++;					// move to next slot
//		} else {
//			printf("done\n");
//
//			printit();
//		}
//
//	}

	newByte	= 0;
	newBits	= 0;
	prevBit = 0;
	sync	= 0;
	scnt	= 0;

	while(1) {
		WORD val = LPC_TMR32B0->IR;		// get interrupt status

		if(val & 0x10) {				// is CR0INT set?
			LPC_TMR32B0->IR = 0x10;		// clear CR0INT

			val = LPC_TMR32B0->CR0;		// read CR0
			LPC_TMR32B0->TC = 0;

			if(cnt == 2048) {			// if have 2048 values
				printf("done\n");
				printit();
			}

			//--------------
			if(val < 360) {			// 4 is bellow 360
				val = 4;

				appendBit(1);
			} else if(val < 500) {	// 6 is above 360 and bellow 500
				val = 6;

				if(prevBit == 0) {
					appendBit(0);
					appendBit(1);
				} else {
					appendBit(0);
				}
			} else {				// 5 is above 500
				val = 8;

				appendBit(0);
				appendBit(1);
			}

			sync = sync << 8;
			sync = sync | ((DWORD) val);

			if(sync == 0x08060806) {
				scnt++;
				sync = 0;
				newBits = 0;
			}

			//				times[cnt] = val;		// store
			//				cnt++;					// move to next slot
		}

	}
	return 0 ;
}

void appendBit(BYTE bit)
{
	newByte = newByte << 1;

	if(bit != 0) {
		newByte = newByte | 0x01;
	}

	newBits++;

	if(newBits == 8) {
		times[cnt] = newByte;		// store
		cnt++;						// move to next slot
		newBits = 0;
	}

	prevBit = bit;
}

void printit(void)
{
	printf("Found syncs: %d\n", (int)scnt);

	WORD i;
	for(i=0; i<2048; i++) {
//		printf("%d\n", (int)times[i]);
		printf("%02x\n", (int)times[i]);
	}

	while(1);
}

void init_timer32(void)
{
	// Some of the I/O pins need to be carefully planned if you use below module because JTAG and TIMER CAP/MAT pins are muxed.

	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<9);	// enable clock for CT32B0

	LPC_IOCON->PIO1_5 &= ~0x07;				// remove 3 lowest bits of this IOCON
	LPC_IOCON->PIO1_5 |=  0x02;				// set function of PIO1_5 to Timer0_32 CAP0

	// LPC_TMR32B0->CTCR = 0 after reset; that is -- Timer Mode: every rising PCLK edge

	LPC_TMR32B0->CCR = 0x06;				// Capture on CT32Bn_CAP0 falling edge, Interrupt on CT32Bn_CAP0 event

	// LPC_TMR32B0->PR  -- prescale, 0= TC++ on every PCLK, 1= TC++ on every 2 PCLKs, ...
//	LPC_TMR32B0->PR = 35;					// prescale of TC++. When CLK is 72 MHz and prescale is 35, it should prescale TC++ to 0.5 us

	// LPC_TMR32B0->IR  -- bit 4 - CR0INT - Interrupt flag for capture channel 0 event.
	// LPC_TMR32B0->CR0 -- Capture Register. CR0 is loaded with the value of TC when there is an event on the CT32B0_CAP0 input.

	LPC_TMR32B0->TCR = 0x01;				// enable TC
}

void setupClock(void)
{
	LPC_SYSCON->SYSOSCCTRL = 0;			// 1-20MHz range + Oscillator not bypassed
	LPC_SYSCON->PDRUNCFG &= ~(1<<5);	// power up system oscillator
	LPC_SYSCON->SYSPLLCLKSEL = 1;		// System oscillator is input for the PLL
	LPC_SYSCON->SYSPLLCLKUEN = 0;		// clear SYSPLLCLKUEN
	LPC_SYSCON->SYSPLLCLKUEN = 1;		// start pll input clock change
	LPC_SYSCON->SYSPLLCTRL = 0x25;		// PLL P=2, M=6 for CLKIN x 6 = 72 MHz
	LPC_SYSCON->PDRUNCFG &= ~(1<<7);	// power up system pll

	while(!(LPC_SYSCON->SYSPLLSTAT & 0x01));	// Wait for PLL to lock

	LPC_SYSCON->MAINCLKSEL = 3;			// main clock = system pll clock out
	LPC_SYSCON->MAINCLKUEN = 0;			// clear MAINCLKUEN
	LPC_SYSCON->MAINCLKUEN = 1;			// start main clock change to pll output

	LPC_SYSCON->SYSAHBCLKDIV = 1;		// core/peripheral clock = main clock divided by 1
}
