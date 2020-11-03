/*Modificar el ejemplo del timer adjunto para que por cada presión
 *de un pulsador la frecuencia de parpadeo disminuya a la mitad
 *debido a la modificación del reloj del periférico.
 *El pulsador debe producir una interrupción por GPIO0
 *con flanco descendente*/


#include "lpc17xx.h"
#include <stdio.h>

void config_pins();
void config_EINT3();
void config_TMR0();



int main(){
	SystemInit();
	config_pins();
	config_EINT3();
	config_TMR0();

	while(1);
}

void config_pins(){

	LPC_GPIO0->FIODIR  &=~(1<<10);  //p0.10  como input
	LPC_GPIO0->FIODIR   |= 0x0f;    //s0,s1,s2,s3 outputs
	LPC_GPIO1->FIODIR   &=~(1<<26); //pin1.26 como input

	LPC_PINCON->PINSEL3 |= (3<<20); //pin1.26 como cap0.00

	return;
}

void config_EINT3(){
	LPC_GPIOINT->IO0IntClr  = (1<<10);
	LPC_GPIOINT->IO0IntEnF  = (1<<10);
	NVIC_EnableIRQ(EINT3_IRQn);

	return;
}

void config_TMR0(){

	LPC_SC->PCONP    |= 0x01;        //enable TMR0
	LPC_SC->PCLKSEL0 &= ~(3<<2);     //pclk = clk/2
	LPC_TIM0->PR      = 25000-1;     //cuenta cada 1ms

	LPC_TIM0->CCR = 0x07; // cap falling y rising edge + interrupcion
	LPC_TIM0->MCR = 0x00; //deshabilita interrupciones por match

	LPC_TIM0->TCR = 0x03;
	LPC_TIM0->TCR&=~(0x02);
	LPC_GPIOINT->IO0IntClr  = (1<<10);
	NVIC_EnableIRQ(TIMER0_IRQn);

	return;
}

void EINT3_IRQHandler(){
	printf("\nentre a EINT3 handler");

/*	LPC_TIM0->TCR = 0x03;
	LPC_TIM0->TCR&=~(0x02);
	LPC_GPIOINT->IO0IntClr  = (1<<10);
	NVIC_EnableIRQ(TIMER0_IRQn);*/

}
/* @brief: medicion de duty cycle de cada color
 */
void TIMER0_IRQHandler(){
	static uint8_t  count = 0;
	static uint32_t value0 = 0;
	static uint32_t value1 = 0;
	printf("\nentre en handler");

	if(count == 0){
		value0 = LPC_TIM0->CR0;
		count++;
	}else{
		value1 = LPC_TIM0->CR0;
		count = 0;
		printf("\ndelta tiempo = %ld",(value1-value0));

	}

	LPC_TIM0->IR |= LPC_TIM0->IR;
	return;

}

