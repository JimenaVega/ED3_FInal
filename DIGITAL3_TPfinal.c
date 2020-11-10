/*
===============================================================================
 Name        : DIGITAL3_TPfinal.c
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

/**********************************defines************************************/

#define DELAY_AR 		100
#define RED				1	//multiplexor filtros sensor
#define GREEN			2
#define BLUE			3

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
#define PLAY_init       0x3F 	// iniciacion del modulo

//format
#define CMD_LINE_SIZE   10
#define START_BYTE		0x7E
#define VERSION_BYTE	0xFF
#define COMMAND_LENGTH	0x06
#define ACKNOWLEDGE		0x00
#define END_BYTE		0xEF


/*************************function prototypes*********************************/

void initUART(void);		//comunicacion con el DFPlayer
void initGPIO(void);		//multiplexación filtros sensor, leds
void initEXTI(void);		//pulsador para disparar el proceso de deteccion de color
void initSysTick(void);		//inicializacion del systick para control de rebote
void initTMR0(void);		//timer0 se utilizara para determinar el duty cycle salida del sensor
void initSensor(void);		//se inicializa el funcionamiento del sensor


//DFplayer module functions
void sendCommand(uint8_t cmd, uint8_t part1, uint8_t part2); //trama de envio comandos


/**************************global variables***********************************/

int detectColor_flag=DISABLE;	//bandera que se pondrá en alto si se presiona el pulsador, indica que se debe detectar el color
int tmr_OnOff       =DISABLE;	//bandera que indica si el timer estaba contando o no
int actual_filter   =RED;		//indica el filtro activo
int done            =DISABLE;	//bandera indica si ya se conoce el resultado de todos los canales

uint32_t red_freq	;		//almacena la frecuencia detectada por el filtro rojo
uint32_t green_freq	;		//almacena la frecuencia detectada por el filtro verde
uint32_t blue_freq	;		//almacena la frecuencia detectada por el filtro azul

/*******************************main******************************************/

int main(void) {

	//INICIALIZACIONES MÓDULOS
	SystemInit();

	initUART();
	initTMR0();
	initSysTick();
	initSensor();
	initGPIO();
	initEXTI();

	//INICIALIZACION DFPLAYER
	sendCommand(PLAY_init, 0, 0);					//se inicia el dispositivo
	sendCommand(SPECIFY_VOL, 0, 15);				//volumen medio (de 0 a 30)



	while(1){
		if (done==ENABLE && detectColor_flag==ENABLE){
			if (red_freq>green_freq && red_freq>blue_freq){
				GPIO_ClearValue(0, 3<<16);
				GPIO_SetValue  (0, 1<<15);			//encender led rojo

				sendCommand(SPECIFY_FOLDER, 0, 1); 	//se elige la carpeta 1(nombrarla asi)
				delay();							//no se si es suficiente o hace falta mayor delay
				sendCommand(SPECIFY_TRACK, 0, 1);	//se elige el primer track de la carpeta(nombrarlo asi)
			}
			else if (green_freq>red_freq && green_freq>blue_freq){
				GPIO_ClearValue(0, 5<<15);
				GPIO_SetValue  (0, 1<<16);			//encender led verde

				sendCommand(SPECIFY_FOLDER, 0, 1); 	//se elige la carpeta 1(nombrarla asi)
				delay();							//no se si es suficiente o hace falta mayor delay
				sendCommand(SPECIFY_TRACK, 0, 2);	//se elige el segundo track de la carpeta(nombrarlo asi)
			}
			else if (blue_freq>red_freq && blue_freq>green_freq){
				GPIO_ClearValue(0, 3<<15);
				GPIO_SetValue  (0, 1<<17);			//encender led azul

				sendCommand(SPECIFY_FOLDER, 0, 1); 	//se elige la carpeta 1(nombrarla asi)
				delay();							//no se si es suficiente o hace falta mayor delay
				sendCommand(SPECIFY_TRACK, 0, 3);	//se elige el tercer track de la carpeta(nombrarlo asi)
			}

			detectColor_flag=DISABLE; 			//deteccion finalizada
		}
	}


    return 0 ;
}

/******************************functions**************************************/

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

	UART_TxCmd(LPC_UART0, ENABLE);					//Enable transmission on UART TxD pin
	return;
}

void initGPIO(void){

	//se configuran los pines de salida GPIO

	PINSEL_CFG_Type confGPIO;

	confGPIO.Funcnum  =0;
	confGPIO.OpenDrain=PINSEL_PINMODE_NORMAL;
	confGPIO.Pinmode  =PINSEL_PINMODE_PULLUP;
	confGPIO.Portnum  =0;

		//pines destinados a control del sensor, deben ser salidas de GPIO
	confGPIO.Pinnum = 6;				//S0
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 7;				//S1
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 8;				//S2
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 9;				//S3
	PINSEL_ConfigPin(&confGPIO);

	GPIO_SetDir(0, (0xF<<6), 1);        //pines puerto 0 del 6 al 9 como salidas
	GPIO_ClearValue(0, (0xF<<6));		//se inicializan todos en bajo

		//pines destinados a leds de verificación
	confGPIO.Pinnum = 15;				//ROJO
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 16;				//VERDE
	PINSEL_ConfigPin(&confGPIO);
	confGPIO.Pinnum = 17;				//AZUL
	PINSEL_ConfigPin(&confGPIO);

	GPIO_SetDir(0, (0x7<<15), 1);        //pines puerto 0 del 15 al 17 como salidas
	GPIO_ClearValue(0, (0x7<<15));		 //se inicializan todos en bajo

		//pin destinado a la salida del sensor, se configura como cap0.0
	confGPIO.Funcnum = 3;
	confGPIO.Portnum = 1;
	confGPIO.Pinnum  = 26;
	PINSEL_ConfigPin(&confGPIO);

	return;
}

void initEXTI(void){

	//configuracion pin interrupcion externa 0 (pulsador)
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

	NVIC_EnableIRQ(EINT0_IRQn);

	return;
}

void initSysTick(void){

	//configuracion destinada a delay de 100ms para eliminar efectos de rebote
	SYSTICK_InternalInit(DELAY_AR);
	SYSTICK_ClearCounterFlag();

	return;
}

void initTMR0(void){
	TIM_TIMERCFG_Type confTMR0;

	confTMR0.PrescaleOption=TIM_PRESCALE_TICKVAL;
	confTMR0.PrescaleValue =25000;  				//cambiar valor en lpc17xx_timer.h o recalcular para clk/4

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &confTMR0);

	TIM_CAPTURECFG_Type confCAPTURE;

	confCAPTURE.CaptureChannel=TIM_COUNTER_INCAP0;
	confCAPTURE.FallingEdge=ENABLE;
	confCAPTURE.IntOnCaption=ENABLE;
	confCAPTURE.RisingEdge=ENABLE;

	TIM_ConfigCapture(LPC_TIM0, &confCAPTURE); 		//no se enciende el timer, iniciará la cuenta una vez que sea detectado el flanco del capture y se apagara con el segundo

	return;
}

void delay(void){

	//inicia la cuenta del systick y se habilitan las interrupciones
	SYSTICK_IntCmd(ENABLE);
	SYSTICK_Cmd(ENABLE);

	return;
}

void sendCommand(uint8_t cmd, uint8_t part1, uint8_t part2){
	//checksum calcule
	uint16_t checksum;
	uint8_t  checksum_high;
	uint8_t  checksum_low;

	checksum = -(VERSION_BYTE + COMMAND_LENGTH + cmd + ACKNOWLEDGE + part1 + part2);
	checksum_high=(0xFF00 & checksum)>>8;
	checksum_low =(0x00FF & checksum);

	//armado de la trama de transmision
	uint8_t command_line[10] = {START_BYTE, VERSION_BYTE, COMMAND_LENGTH, cmd, ACKNOWLEDGE, part1, part2, checksum_high, checksum_low, END_BYTE};

	//envío del comando byte por byte
	for (int i=0; i<CMD_LINE_SIZE; i++){
		UART_SendByte(LPC_UART0, command_line[i]);
	}
	return;
}

void initSensor(void){
	GPIO_SetValue(0, 1<<6);		//frequency scaling to 20% -> output typically up to 12kHz
	GPIO_ClearValue(0, 3<<8);	//S2 y S3 en bajo -> filtro rojo seleccionado
	return;
}

/***********************************handlers*********************************/

void SysTick_Handler(void){
	SYSTICK_Cmd(DISABLE);				//el systick deja de contar
	SYSTICK_IntCmd(DISABLE);			//se deshabilitan las interrupciones
	SYSTICK_ClearCounterFlag();			//se baja la bandera de interrupcion
	return;
}

void EINT0_IRQHandler(void){
	EXTI_ClearEXTIFlag(EXTI_EINT0);		//se limpia la bandera de la interrupción
	detectColor_flag=ENABLE;			//bandera en alto, inicia detección del color
	delay();							//delay para evitar interrupciones por rebote

	TIM_ClearIntCapturePending(LPC_TIM0, TIM_CR0_INT);
	NVIC_EnableIRQ(TIMER0_IRQn);		//se habilitan interrupciones por timer0
	return;
}

void TIMER0_IRQHandler(void){
	TIM_ClearIntCapturePending(LPC_TIM0, TIM_CR0_INT);	//se limpia bandera de interrupcion por cap0.0

	if(tmr_OnOff==DISABLE){
		TIM_Cmd(LPC_TIM0, ENABLE);						//luego de la primera interrupción por timer, se habilita la cuenta
		tmr_OnOff=ENABLE;								//bandera timer encendido
	}
	else{
		TIM_Cmd(LPC_TIM0, DISABLE);						//luego de la segunda interrupcion por capture, se deshabilita la cuenta
		tmr_OnOff=DISABLE;								//bandera timer apagado

		switch(actual_filter){
		case(RED):
				red_freq=TIM_GetCaptureValue(LPC_TIM0, TIM_COUNTER_INCAP0);
				GPIO_SetValue(0, 3<<8);					//S2 y S3 en alto
				actual_filter=GREEN;
				delay();
				break;
		case(GREEN):
				green_freq=TIM_GetCaptureValue(LPC_TIM0, TIM_COUNTER_INCAP0);
				GPIO_ClearValue(0, 1<<8);				//S2 en bajo
				GPIO_SetValue  (0, 1<<9);				//S3 en alto
				actual_filter=BLUE;
				delay();
				break;
		case(BLUE):
				blue_freq=TIM_GetCaptureValue(LPC_TIM0, TIM_COUNTER_INCAP0);
				GPIO_ClearValue(0, 3<<8);				//S2 y S3 en bajo
				actual_filter=RED;
				done =ENABLE;							//bandera que indica que ya se tienen los resultados de todos los filtros
				NVIC_DisableIRQ(TIMER0_IRQn);			//si ya se determino la salida de cada filtro, se deshabilitan las interruciones por timer0
				break;
		}
		TIM_ResetCounter(LPC_TIM0); 					//se resetea la cuenta del timer
	}

	return;
}
