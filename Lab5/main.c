#include "msp.h"
#include <driverlib.h>
#include "G8RTOS_Scheduler.h"
#include "G8RTOS_Semaphores.h"
#include "Game.h"
#include "LCDLib.h"

//extern semaphore_t sem_sensor;
//extern semaphore_t sem_LED;
//extern semaphore_t sem_count;
extern semaphore_t sensorMutex;
extern semaphore_t ballMutex;
extern semaphore_t LEDMutex;
extern semaphore_t LCDMutex;

uint32_t counter0;
uint32_t counter1;
uint32_t counter2;
threadId_t task0_ID;
threadId_t task1_ID;
threadId_t task2_ID;

void task0(void){
    task0_ID = G8RTOS_GetThreadId();
    while(1){
        counter0++;
    }
}
void task1(void){
    task1_ID = G8RTOS_GetThreadId();
    while(1){
        counter1++;
        if(counter1 > 1000){
            G8RTOS_AddThread(task0, 3, "task 0 high");
            G8RTOS_KillSelf();
        }
    }
}
void task2(void){
    task2_ID = G8RTOS_GetThreadId();
    while(1){
        counter2++;
        if(counter0 > 1000){
            counter0 = 0;
            G8RTOS_KillThread(task0_ID);
        }
    }
}

/* Configuration for UART */
static const eUSCI_UART_Config Uart115200Config = {
    EUSCI_A_UART_CLOCKSOURCE_SMCLK, // SMCLK Clock Source
    6, // BRDIV
    8, // UCxBRF
    0, // UCxBRS
    EUSCI_A_UART_NO_PARITY, // No Parity
    EUSCI_A_UART_LSB_FIRST, // LSB First
    EUSCI_A_UART_ONE_STOP_BIT, // One stop bit
    EUSCI_A_UART_MODE, // UART mode
    EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION // Oversampling
};

void uartInit(){
    /* select the GPIO functionality */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

    /* configure the digital oscillator */
    CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_12);

    /* configure the UART with baud rate 115200 */
    MAP_UART_initModule(EUSCI_A0_BASE, &Uart115200Config);

    /* enable the UART */
    MAP_UART_enableModule(EUSCI_A0_BASE);
}

void PORT4_IRQHandler(void){
    //__NVIC_DisableIRQ(PORT4_IRQn);
    //P4->IE &= ~BIT0;

    // set interrupt flag
    //LCDTap();

    // All the following are done in WaitForTap()
    //P4->IFG &= ~BIT0; // must clear IFG flag, can also read PxIV to clear IFG
    //P4->IE |= BIT0;
    //__NVIC_EnableIRQ(PORT4_IRQn);
}

/**
 * main.c
 */
void main(void)
{
	WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;		// stop watchdog timer

	G8RTOS_Init();

    uartInit();

    LCD_Init(1);

    initCC3100(Client);

    P4->DIR &= ~BIT4;
    P4->IFG &= ~BIT4; // P4.4 IFG cleared
    P4->IES |= BIT4;  // High-to-low transition
    P4->REN |= BIT4; // Pull-up resistor
    P4->OUT |= BIT4; // Sets res to pull-up

    //    G8RTOS_AddThread(CreateGame, 4, "create");
    //G8RTOS_AddAPeriodicEvent(buttonPress, 4, PORT4_IRQn);

    G8RTOS_InitSemaphore(&sensorMutex, 1);
    G8RTOS_InitSemaphore(&ballMutex, 1);
    G8RTOS_InitSemaphore(&LCDMutex, 1);
    G8RTOS_InitSemaphore(&LEDMutex, 1);

	G8RTOS_Launch();
}
