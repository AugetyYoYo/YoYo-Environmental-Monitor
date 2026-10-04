#include "STC8H.H"
#include "keypad.h"
#include "led.h"

void KEYPAD_INIT()
{
	P5M1 &= ~((1<<1) | (1<<2) | (1<<4));
  P5M0 &= ~((1<<1) | (1<<2) | (1<<4));
}

void KF4_Interrupt_Init(void)
{
	P_SW2 |=  0x80;
	
	P5IM1 |= (1<<4);
	P5IM0 &= ~(1<<4);

	P5INTE |= (1<<4);
	
	P5INTF &= ~(1 << 4);
}

void KF4INT_ISR(void) interrupt 42
{
	if(P5INTF & (1<<4))
	{
		P5INTF &= ~(1<<4);
		LED0 = 1;    
//		LED0 = !LED0;
	}
}