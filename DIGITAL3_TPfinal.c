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

#define DELAY_AR 		100		//uso delay de 100 ms
#define RED				1		//multiplexor filtros sensor
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

//formato trama de transmisión de los comandos
#define CMD_LINE_SIZE   10
#define START_BYTE		0x7E
#define VERSION_BYTE	0xFF
#define COMMAND_LENGTH	0x06
#define ACKNOWLEDGE		0x00
#define END_BYTE		0xEF


/*************************function prototypes*********************************/

void initGPIO(void);			//multiplexación filtros sensor, leds, capture0.0
void initEXTI(void);			//pulsador para disparar el proceso de deteccion de color
void initSysTick(void);			//inicializacion del systick para control de rebote
void initTMR0(void);			//timer0 se utilizara para determinar el duty cycle salida del sensor
void initSensor(void);			//se inicializa el funcionamiento del sensor
void initUART(void);			//comunicacion con el DFPlayer
void initDFplayer(void);		//inicializacón módulo DFplayer
void delay(int delayValue);		//delay multiplo de 100 ms
void sendCommand(uint8_t cmd, uint8_t part1, uint8_t part2); //trama de envio comandos


/**************************global variables***********************************/

int detectColor_flag=DISABLE;	//bandera que se pondrá en alto si se presiona el pulsador, indica que se debe detectar el color
int tmr_OnOff       =DISABLE;	//bandera que indica si el timer estaba contando o no
int actual_filter   =RED;		//indica el filtro activo
int done            =DISABLE;	//bandera indica si ya se conoce el resultado de todos los canales
int delayValue		;			//cuando es 1: delay de 100ms. Cuando es 2: delay de 200ms, etc
int whishedDelay    ;			//variable para almacenar el valor de delay deseado (no cambia, por otro lado, delayValue va aumentando)

uint32_t red_freq	;			//almacena la frecuencia detectada por el filtro rojo
uint32_t green_freq	;			//almacena la frecuencia detectada por el filtro verde
uint32_t blue_freq	;			//almacena la frecuencia detectada por el filtro azul

/*******************************main******************************************/

/*
	INICIALIZACIÓN: Clock del sistema
			   	    Modulos y perifericos usados

	WHILE(1)	  : Una vez que se tienen las 3 intensidades de cada filtro, se busca el predominante
					Se enciende el led correspondiente
					Se reproduce el audio correspondiente
					Entre comandos enviados se agrega un delay de 500ms para el tiempo de procesamiento del módulo
					Una vez hecho todo lo anterior, se bajan banderas de "done" y "detectColor_flag"

	NOTA          : Dentro de la tarjeta SD, se nombra una carpeta como "1"
					Dicha carpeta contiene los audios
					Audio "1": rojo
					Audio "2": verde
					Audio "3": azul

*/
int main(void) {

	SystemInit();

	initGPIO();
	initEXTI();
	initSysTick();
	initTMR0();
	initSensor();
	initUART();
	initDFplayer();


	while(1){
		if (done==ENABLE && detectColor_flag==ENABLE){
			if (red_freq>green_freq && red_freq>blue_freq){			//color rojo detectado
				GPIO_ClearValue(0, 3<<16);
				GPIO_SetValue  (0, 1<<15);

				sendCommand(SPECIFY_FOLDER, 1, 1); 					//se elige la carpeta 1(nombrarla "01"), se elige el primer track de la carpeta(nombrarlo como "001")
				delay(5);
			}
			else if (green_freq>red_freq && green_freq>blue_freq){	//color verde detectado
				GPIO_ClearValue(0, 5<<15);
				GPIO_SetValue  (0, 1<<16);

				sendCommand(SPECIFY_FOLDER, 1, 2); 					//se elige la carpeta 1(nombrarla "01"),se elige el segundo track de la carpeta(nombrarlo como "002")
				delay(5);

			}
			else if (blue_freq>red_freq && blue_freq>green_freq){	//color azul detectado
				GPIO_ClearValue(0, 3<<15);
				GPIO_SetValue  (0, 1<<17);

				sendCommand(SPECIFY_FOLDER, 1, 3); 					//se elige la carpeta 1(nombrarla "01"), se elige el tercer track de la carpeta(nombrarlo como "003")
				delay(5);
			}

			detectColor_flag=DISABLE;
			done=DISABLE;
		}
	}

    return 0 ;
}

/******************************functions**************************************/

/*
  CONFIGURACION: Puerto UART3 para controlar módulo DFPlayer
  	  	  	  	 Pinsel para configuar Tx y Rx correspondientes al UART0
  	  	  	  	 Baudrate 9600 valor de trabajo del módulo
  	  	  	  	 Se habilita la transmision Tx
*/


void initUART(void){

	PINSEL_CFG_Type confUART_PIN;

	confUART_PIN.Funcnum  =2;
	confUART_PIN.OpenDrain=PINSEL_PINMODE_NORMAL;
	confUART_PIN.Pinmode  =PINSEL_PINMODE_PULLUP;
	confUART_PIN.Portnum  =0;

		//Tx
	confUART_PIN.Pinnum=0;
	PINSEL_ConfigPin(&confUART_PIN);

		//Rx
	confUART_PIN.Pinnum=1;
	PINSEL_ConfigPin(&confUART_PIN);


	UART_CFG_Type confUART;

	confUART.Baud_rate=9600;
	confUART.Databits =UART_DATABIT_8;
	confUART.Parity   =UART_PARITY_NONE;
	confUART.Stopbits =UART_STOPBIT_1;

	UART_Init(LPC_UART3, &confUART);

	UART_FIFO_CFG_Type confUART_FIFO;

	confUART_FIFO.FIFO_DMAMode=DISABLE;
	confUART_FIFO.FIFO_Level  =UART_FIFO_TRGLEV0;
	confUART_FIFO.FIFO_ResetRxBuf=ENABLE;
	confUART_FIFO.FIFO_ResetTxBuf=ENABLE;

	UART_FIFOConfig(LPC_UART3, &confUART_FIFO);

	UART_TxCmd(LPC_UART3, ENABLE);					//Enable transmission on UART TxD pin
	return;
}


/*
  CONFIGURACION: OUTPUT pines para control del sensor: S0 y S1 output frequency scaling
  	  	  	  	  	  	  	  	  	  	  	  		   S2 y S3 photodiode type (seleccion del filtro)
 	 	 	 	 OUTPUT pines para leds de verificación
 	 	 	 	 CAPTURE (INPUT) pin de salida del sensor
*/

void initGPIO(void){

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

/*
  CONFIGURACION: pin interrupcion externa 0 (pulsador)
  NOTA         : Se configura con resistencia pull up, la interrupcion es activa por flanco de bajada
  			     Interrupción de menor prioridad que la de TMR0 (proceso de detección del color)
*/

void initEXTI(void){

	PINSEL_CFG_Type confEXTI_PINS;

	confEXTI_PINS.Funcnum  =1;
	confEXTI_PINS.OpenDrain=PINSEL_PINMODE_NORMAL;
	confEXTI_PINS.Pinmode  =PINSEL_PINMODE_PULLUP;
	confEXTI_PINS.Portnum  =2;
	confEXTI_PINS.Pinnum   =10;
	PINSEL_ConfigPin(&confEXTI_PINS);


	EXTI_InitTypeDef confEXTI;

	confEXTI.EXTI_Line=EXTI_EINT0;
	confEXTI.EXTI_Mode=EXTI_MODE_EDGE_SENSITIVE;
	confEXTI.EXTI_polarity=EXTI_POLARITY_LOW_ACTIVE_OR_FALLING_EDGE;
	EXTI_Config(&confEXTI);

	NVIC_SetPriority(EINT0_IRQn, 3);
	NVIC_EnableIRQ(EINT0_IRQn);

	return;
}

/*
  CONFIGURACION: Genera interrupcion cada 100ms cuando se inicia la cuenta
  	  	  	  	 Se inicializa con interrupciones y cuenta desactivada
  	  	  	  	 Prioridad mayor a la de las interrupciones externas y por timer0 (tiempo para que los procesos desencadenados por estos dos funcionen correctamente)

  NOTA   	   : Destinado a delay para eliminar efectos de rebote

*/

void initSysTick(void){

	SYSTICK_InternalInit(DELAY_AR);
	SYSTICK_IntCmd(DISABLE);
	SYSTICK_Cmd(DISABLE);
	SYSTICK_ClearCounterFlag();

	NVIC_SetPriority(SysTick_IRQn, 1);

	return;
}


/*
  CONFIGURACION: Timer0 funcionando como contador con entrada de capture, período de 1us
  	  	  	  	 Interrupciones en flancos de subida y bajada del pin de capture
  	  	  	  	 Se usa el capture para determinar el período de la salida del sensor
  	  	  	  	 Prioridad de interrupcion mayor a la interrupcion externa 0

  NOTA         : No se enciende el timer, se iniciará la cuenta una vez que sea detectado el flanco del capture y se apagara con el segundo
  	  	  	  	 Aún no se habilita el manejo de interrupciones por el NVIC

*/

void initTMR0(void){
	TIM_TIMERCFG_Type confTMR0;

	confTMR0.PrescaleOption=TIM_PRESCALE_TICKVAL;
	confTMR0.PrescaleValue =25;

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &confTMR0);

	TIM_CAPTURECFG_Type confCAPTURE;

	confCAPTURE.CaptureChannel=TIM_COUNTER_INCAP0;
	confCAPTURE.FallingEdge=ENABLE;
	confCAPTURE.IntOnCaption=ENABLE;
	confCAPTURE.RisingEdge=ENABLE;

	TIM_ConfigCapture(LPC_TIM0, &confCAPTURE);

	NVIC_SetPriority(TIMER0_IRQn, 2);
	NVIC_DisableIRQ(TIMER0_IRQn);

	return;
}

/*
  CONFIGURACION: control del sensor: S0 y S1 output frequency scaling to 20% -> output typically up to 12kHz
  	  	  	  	  	  	  	  	  	 S2 y S3 photodiode type en bajo -> filtro rojo seleccionado

*/

void initSensor(void){
	GPIO_SetValue(0, 1<<6);
	GPIO_ClearValue(0, 3<<8);
	return;
}

/*
  CONFIGURACION: Inicia la cuenta del systick y se habilitan las interrupciones
				 Delay 100ms = no entra al while ya que se logra en la primer interrupcion
				 Delay mayor a 100ms = entra al while
*/

void delay(int delayValue){

	SYSTICK_IntCmd(ENABLE);
	SYSTICK_Cmd(ENABLE);
	whishedDelay=delayValue;

	if (delayValue != 1){
		while(delayValue<(2*whishedDelay)){}
	}
	return;
}


/*
  CONFIGURACION: Inicialización del módulo DFPlayer
  	  	  	  	 Configuración volumen medio (puede ir de 0 a 30, se configura como 15)

*/

void initDFplayer(void){
	sendCommand(PLAY_init, 0, 0);
	delay(5);
	sendCommand(SPECIFY_VOL, 0, 15);
	delay(5);
	return;
}


/*
  CONFIGURACION: Trama de transmisión de los comandos de control del módulo DFPlayer

*/

void sendCommand(uint8_t cmd, uint8_t part1, uint8_t part2){
	uint16_t checksum;
	uint8_t  checksum_high;
	uint8_t  checksum_low;

	checksum = -(VERSION_BYTE + COMMAND_LENGTH + cmd + ACKNOWLEDGE + part1 + part2);
	checksum_high=(0xFF00 & checksum)>>8;
	checksum_low =(0x00FF & checksum);

	//armado de la trama de transmision
	uint8_t command_line[CMD_LINE_SIZE] = {START_BYTE, VERSION_BYTE, COMMAND_LENGTH, cmd, ACKNOWLEDGE, part1, part2, checksum_high, checksum_low, END_BYTE};

	//envío del comando byte por byte
	UART_Send(LPC_UART3, command_line, sizeof(command_line), BLOCKING);

	return;
}



/***********************************handlers*********************************/

/*
  MANEJO: Presión del pulsador.
  	  	  Debe iniciar el proceso de detección del color, para indicarlo, se pone en alto "detectColor_flag"
  	  	  Incorpora el uso de un delay de 200ms para evitar interrupciones por rebote
  	  	  Habilita las interrupciones por timer0. Previamente limpia la bandera de la interrupcion

  NOTA  : Solamente una vez que se presionó el botón y hasta que se termine la detección del color se habilitan las interrupciones por timer0
  	  	  Si el boton no se presiona, por mas que se tenga salida del sensor, no producen interrupciones
*/

void EINT0_IRQHandler(void){
	EXTI_ClearEXTIFlag(EXTI_EINT0);
	detectColor_flag=ENABLE;
	delay(2);

	TIM_ClearIntCapturePending(LPC_TIM0, TIM_CR0_INT);
	NVIC_EnableIRQ(TIMER0_IRQn);
	return;
}

/*

  MANEJO: Una vez que pasaron los ms de delay: el systick deja de contar
  	  	  	  	  	  	  	  	  	  	  	   se deshabilitan las interrupciones
  	  	  	  	  	  	  	  	  	  	  	   se baja la bandera de interrupcion

*/

void SysTick_Handler(void){
	if(delayValue==1 || delayValue==(2*whishedDelay-1)){
		SYSTICK_Cmd(DISABLE);
		SYSTICK_IntCmd(DISABLE);
		delayValue=2*whishedDelay;
	}
	else if (delayValue>=whishedDelay && delayValue<(2*whishedDelay-1)){
		delayValue++;
	}

	SYSTICK_ClearCounterFlag();

	return;
}


/*
  MANEJO: Se entra a la ISR cuando se presiona el botón y se detecta un flanco en el pin de capture
  	  	  Si se detecta el primer flanco, se inicia la cuenta del timer
  	  	  Si se detecta el segundo flanco, se finaliza la cuenta del timer y se lo reinicia

  	  	  Una vez detectado el segundo flanco: 	Se cambia el filtro cambiando el estado de los pines S2 y S3 (secuencia de filtros: ROJO, VERDE AZUL)
  	  	  	  	  	  	  	  	  	  	  	  	Se almacena la cuenta del timer en el registro correspondiente a cada filtro

  	  	  Una vez se tomaron los resultados de la salida de cada filtro: Se cambia el filtro nuevamente a rojo
  	  	     	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 Se pone en alto la bandera "done", indica que ya se tomaron los valores necesarios
  	  	     	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 Se deshabilitan las interrupciones por timer0 hasta que se presione nuevamente el botón

  NOTA  : Se determina el período(y por defecto la frecuencia) midiendo el tiempo entre dos flancos de la señal cuadrada
   	   	  "tmr_OnOff" indica si se trata del primer flanco o el segundo
*/

void TIMER0_IRQHandler(void){
	TIM_ClearIntCapturePending(LPC_TIM0, TIM_CR0_INT);

	if(tmr_OnOff==DISABLE){
		TIM_Cmd(LPC_TIM0, ENABLE);
		tmr_OnOff=ENABLE;
	}
	else{
		TIM_Cmd(LPC_TIM0, DISABLE);
		tmr_OnOff=DISABLE;

		switch(actual_filter){
		case(RED):
				red_freq=TIM_GetCaptureValue(LPC_TIM0, TIM_COUNTER_INCAP0);
				GPIO_SetValue(0, 3<<8);					//S2 y S3 en alto (filtro verde)
				actual_filter=GREEN;
				delay(1);
				break;
		case(GREEN):
				green_freq=TIM_GetCaptureValue(LPC_TIM0, TIM_COUNTER_INCAP0);
				GPIO_ClearValue(0, 1<<8);				//S2 en bajo (filtro azul)
				GPIO_SetValue  (0, 1<<9);				//S3 en alto
				actual_filter=BLUE;
				delay(1);
				break;
		case(BLUE):
				blue_freq=TIM_GetCaptureValue(LPC_TIM0, TIM_COUNTER_INCAP0);
				GPIO_ClearValue(0, 3<<8);				//S2 y S3 en bajo (filtro rojo)
				actual_filter=RED;
				delay(1);
				done =ENABLE;
				NVIC_DisableIRQ(TIMER0_IRQn);
				break;
		}
		TIM_ResetCounter(LPC_TIM0);
	}

	return;
}

