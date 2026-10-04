#include "STC8G_H_GPIO.h"
#include "motor.h"

void MOTOR_ON()
{
	MOTOR = 1;
}

void MOTOR_OFF()
{
	MOTOR = 0;
}

void MOTOR_INIT()
{
	GPIO_InitTypeDef pin_MOTOR = {0};
	
	pin_MOTOR.Mode = GPIO_OUT_PP;
	pin_MOTOR.Pin = GPIO_Pin_1;
	
	GPIO_Inilize(GPIO_P0,&pin_MOTOR);
	
	MOTOR_OFF();
}