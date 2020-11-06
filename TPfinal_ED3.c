/*
===============================================================================
 Name        : TPfinal_ED3.c
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
#include "lpc17xx_systick.h"

//defines
#define DELAY_AR 		100
#define FILTER_1		0	//multiplexor filtros sensor
#define FILTER_2		1
#define FILTER_3		3
	//DFPlayer commands

#define NEXT 			0x01
#define PREVIOUS 		0x02
#define SPECIFY_TRACK	0x03
#define INCR_VOL		0x04
#define DECR_VOL		0x05
#define SPECIFY_VOL		0x06
#define SPECIFY_EQ		0x07
#define SPECIFY_PB_M	0x08
#define SPECIFY_PB_S	0x09
#define STANDBY			0x0A
#define NORMAL_WORK		0x0B
#define RESET			0x0C
#define PLAYBACK		0x0D
#define PAUSE			0x0E
#define SPECIFY_FOLDER	0x0F
#define VOLUME_ADJ		0x10
#define REPEAT			0x11

//function prototypes
void initUART(void);		//comunicacion con el DFPlayer
void initGPIO(void);		//multiplexación filtros sensor, leds
void initEXTI(void);		//pulsador para disparar el proceso de deteccion de color
void initSysTick(void);		//inicializacion del systick para control de rebote
void comandsDFP(void);		//control del módulo
void antirrebote(void);		//antirebote por software para el pulsador

	//DFplayer module functions
void send_cmd(uint8_t cmd, uint16_t high_arg, uint16_t low_arg);
void set_volume(uint16_t volume);
void initSendBuffer(void);

//Global variables
	//DFPlayer
uint8_t send_buf[10]; //trama de envio determinada por la configuracion del modulo
//uint8_t recv_buf[10]; no se si se necesita, me parece que no


int main(void) {
	SystemInit();
	initGPIO();
	initUART();
	initEXTI();
	initSysTick();
	initSendBuffer();

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

void initSysTick(void){

	//configuracion destinada a delay de 100ms para eliminar efectos de rebote
	SYSTICK_InternalInit(DELAY_AR);
	SYSTICK_ClearCounterFlag();

	return;
}

void delay(void){

	//inicia la cuenta del systick y se habilitan las interrupciones
	SYSTICK_IntCmd(ENABLE);
	SYSTICK_Cmd(ENABLE);

	return;
}

void SysTick_Handler(void){
	//se baja la bandera de interrupcion, se detiene la cuenta y se deshabilitan las interrupciones
	SYSTICK_ClearCounterFlag();
	SYSTICK_Cmd(DISABLE);
	SYSTICK_IntCmd(DISABLE);
	return;
}


//se encarga de manejar las interrupciones originadas por el boton:
	//se debe activar la multiplxacion de los puertos gpio
	//se debe iniciar el delay

void EINT0_IRQHandler(void){

	EXTI_ClearEXTIFlag(EXTI_EINT0);

	//se incorpora un delay para evitar rebotes del pulsador generen interrupciones
	delay();

	//multiplexacion filtros del sensor

	return;
}




//DFPLAYER FUNCTIONS
void initSendBuffer(void){
	send_buf[0]=0x7E;			//start byte
	send_buf[1]=0xFF;			//version
	send_buf[2]=0x06;			//data length
	send_buf[4]=0x00;			//no feedback
	send_buf[7]=0xFF;			//
	send_buf[8]=0xDD;			//checksum
	send_buf[9]=0xEF;			//end byte
	return;
}









