/*
===============================================================================
 Name        : ED3_final.c
 Author      : 		$MAMANI, María Emilia
 	 	 	 	 	$VEGA CUEVAS, Silvia Jimena
 Version     :
 Copyright   : $(copyright)
 Description :

	El proyecto se trata de un detector de colores, el cual al ser empleado para
personas ciegas, cuenta con un parlante, el cual emite un audio con el color detectado.

===============================================================================
*/

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_nvic.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"

//function prototypes
void initUART(void);		//comunicacion con el DFPlayer
void initGPIO(void);		//multiplexación filtros sensor, leds
void initEXTI(void);		//pulsador para disparar el proceso de deteccion de color
void initSysTick(void);		//inicializacion del systick para control de rebote
void comandsDFP(void);		//control del módulo
void antirrebote(void);		//antirebote por software para el pulsador


int main(void) {
	SystemInit();

	while(1){}

    return 0 ;
}


//valor de trabajo DFPlayer ajustable, pero por defecto, se mantiene en 9600
void initUART(void){

	//se configuran Tx y Rx correspondientes al UART0

	PINSEL_CFG_Type confUART_PIN;

	confUART_PIN.Funcnum  =1;
	confUART_PIN.OpenDrain=PINSEL_PINMODE_NORMAL;
	confUART_PIN.Pinmode  =PINSEL_PINMODE_PULLUP;
	confUART_PIN.Portnum  =0;

		//Tx
	confUART_PIN.Pinnum=2;
	PINSEL_ConfigPin(&confUART_PIN);

		//Rx
	confUART_PIN.Pinnum=3;
	PINSEL_ConfigPin(&confUART_PIN);

	//configuraciones UART

	UART_CFG_Type confUART;

	confUART.Baud_rate=9600;
	confUART.Databits =UART_DATABIT_8;
	confUART.Parity   =UART_PARITY_NONE;
	confUART.Stopbits =UART_STOPBIT_1;

	UART_Init(LPC_UART0, &confUART);

	UART_FIFO_CFG_Type confUART_FIFO;

	confUART_FIFO.FIFO_DMAMode=DISABLE;
	confUART_FIFO.FIFO_Level  =UART_FIFO_TRGLEV0;
	confUART_FIFO.FIFO_ResetRxBuf=ENABLE;
	confUART_FIFO.FIFO_ResetTxBuf=ENABLE;

	UART_FIFOConfig(LPC_UART0, &confUART_FIFO);

	return;
}

void initGPIO(void){

	//se configuran los pines de salida GPIO

	PINSEL_CFG_Type confGPIO;

	confGPIO.Funcnum  =0;
	confGPIO.OpenDrain=PINSEL_PINMODE_NORMAL;
	confGPIO.Pinmode  =PINSEL_PINMODE_PULLUP;
	confGPIO.Portnum  =0;

		//pines destinados a control del sensor
	confGPIO.Pinnum = 6;
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 7;
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 8;
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 9;
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 10;
	PINSEL_ConfigPin(&confGPIO);

		//pines destinados a leds de verificación
	confGPIO.Pinnum = 15;
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 16;
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 17;
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 18;
	PINSEL_ConfigPin(&confGPIO);

	return;
}

void initEXTI(void){

	//configuracion pin interrupcione externa 0
	//se configura con resistencia pull up
	PINSEL_CFG_Type confEXTI_PINS;

	confEXTI_PINS.Funcnum  =1;
	confEXTI_PINS.OpenDrain=PINSEL_PINMODE_NORMAL;
	confEXTI_PINS.Pinmode  =PINSEL_PINMODE_PULLUP;
	confEXTI_PINS.Portnum  =2;
	confEXTI_PINS.Pinnum   =10;
	PINSEL_ConfigPin(&confEXTI_PINS);

	//configuracion interrupcion externa provpcada por el pulsador
	//al estar con resistencia pull up, la interrupcion es activa por flanco de bajada
	EXTI_InitTypeDef confEXTI;

	confEXTI.EXTI_Line=EXTI_EINT0;
	confEXTI.EXTI_Mode=EXTI_MODE_EDGE_SENSITIVE;
	confEXTI.EXTI_polarity=EXTI_POLARITY_LOW_ACTIVE_OR_FALLING_EDGE;
	EXTI_Config(&confEXTI);
	return;
}

void antirrebote(void){

	return;
}











