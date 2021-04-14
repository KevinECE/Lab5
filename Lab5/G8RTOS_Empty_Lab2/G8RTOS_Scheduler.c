/*
 * G8RTOS_Scheduler.c
 */

/*********************************************** Dependencies and Externs *************************************************************/

#include <stdint.h>
#include "msp.h"
#include "G8RTOS_Scheduler.h"
#include "G8RTOS_Structures.h"
#include <driverlib.h>
#include "BSP.h"
#include "G8RTOS_CriticalSection.h"
#include <string.h>

/*
 * G8RTOS_Start exists in asm
 */
extern void G8RTOS_Start();

/* System Core Clock From system_msp432p401r.c */
extern uint32_t SystemCoreClock;

/*
 * Pointer to the currently running Thread Control Block
 */
extern tcb_t * CurrentlyRunningThread;

/*********************************************** Dependencies and Externs *************************************************************/


/*********************************************** Defines ******************************************************************************/

/* Status Register with the Thumb-bit Set */
#define THUMBBIT 0x01000000

// ICSR
#define ICSR (*((volatile unsigned int*)(0xe000ed04)))
#define ICSR_PENDSVSET (1 << 28)
#define SHPR3 (*((volatile unsigned int*)(0xe000ed20)))
#define PENDSV_PRI (1 << 23)
#define SYSTICK_PRI (1 << 31)

/*********************************************** Defines ******************************************************************************/


/*********************************************** Data Structures Used *****************************************************************/

/* Thread Control Blocks
 *	- An array of thread control blocks to hold pertinent information for each thread
 */
static tcb_t threadControlBlocks[MAX_THREADS];

/* Thread Stacks
 *	- An array of arrays that will act as invdividual stacks for each thread
 */
static int32_t threadStacks[MAX_THREADS][STACKSIZE];

static pe_t periodicEvents[MAX_PERIODICEVENTS];

static fifo_t FIFOs[MAX_FIFOS];

/*********************************************** Data Structures Used *****************************************************************/


/*********************************************** Private Variables ********************************************************************/

/*
 * Current Number of Threads currently in the scheduler
 */
static uint32_t NumberOfThreads;
static uint32_t NumberOfPeriodicEvents;
static uint32_t IDCounter;

/*********************************************** Private Variables ********************************************************************/


/*********************************************** Private Functions ********************************************************************/

/*
 * Initializes the Systick and Systick Interrupt
 * The Systick interrupt will be responsible for starting a context switch between threads
 * Param "numCycles": Number of cycles for each systick interrupt
 */
static void InitSysTick(uint32_t numCycles)
{
	/* Implement this */
    SysTick_Config(numCycles); // if 3000000 ticks per 1 sec, then 3000 ticks per 1 ms
    SysTick_enableInterrupt();
}

/*
 * Chooses the next thread to run.
 * Lab 2 Scheduling Algorithm:
 * 	- Simple Round Robin: Choose the next running thread by selecting the currently running thread's next pointer
 */
void G8RTOS_Scheduler()
{
	/* Implement This */
    tcb_t * TempNextThread = CurrentlyRunningThread->Next_tcb;
    uint8_t nextThreadPriority = UINT8_MAX;
    uint32_t i;
    for(i = 0; i < NumberOfThreads; i++){
        if((TempNextThread->blocked == 0) && !(TempNextThread->asleep)){
            if(TempNextThread->priority < nextThreadPriority){
                CurrentlyRunningThread = TempNextThread;
                nextThreadPriority = CurrentlyRunningThread->priority;
            }
        }
        TempNextThread = TempNextThread->Next_tcb;
    }
}

/*
 * SysTick Handler
 * Currently the Systick Handler will only increment the system time
 * and set the PendSV flag to start the scheduler
 *
 * In the future, this function will also be responsible for sleeping threads and periodic threads
 */
void SysTick_Handler()
{
	/* Implement this */
    SystemTime++;

    // check periodic events to run
    pe_t * ppt = &periodicEvents[0];
    uint32_t j;
    for(j = 0; j < NumberOfPeriodicEvents; j++){
        if(ppt->execute_time == SystemTime){
            ppt->execute_time = ppt->period + SystemTime;
            // Run pThread
            (*ppt->Handler)();
        }
        ppt = ppt->Next_pe;
    }

    // check every thread's sleep count, wake up if sleep count == current system time
    tcb_t * pt = CurrentlyRunningThread;
    uint32_t i;
    for(i = 0; i < NumberOfThreads; i++){
        if(pt->sleep_count == SystemTime){
            pt->sleep_count = 0;
            pt->asleep = false;
        }
        pt = pt->Next_tcb;
    }

    ICSR |= ICSR_PENDSVSET;
}

/*********************************************** Private Functions ********************************************************************/


/*********************************************** Public Variables *********************************************************************/

/* Holds the current time for the whole System */
uint32_t SystemTime;

/*********************************************** Public Variables *********************************************************************/


/*********************************************** Public Functions *********************************************************************/

/*
 * Sets variables to an initial state (system time and number of threads)
 * Enables board for highest speed clock and disables watchdog
 */
void G8RTOS_Init()
{
	/* Implement this */
    SystemTime = 0;
    NumberOfThreads = 0;
    NumberOfPeriodicEvents = 0;
    IDCounter = 0;
    uint32_t newVTORTable = 0x20000000;
    memcpy((uint32_t *)newVTORTable, (uint32_t *)SCB->VTOR, 57*4); // 57 interrupts to copy
    SCB->VTOR = newVTORTable;
    BSP_InitBoard();
}

/*
 * Starts G8RTOS Scheduler
 * 	- Initializes the Systick
 * 	- Sets Context to first thread
 * Returns: Error Code for starting scheduler. This will only return if the scheduler fails
 */
int G8RTOS_Launch()
{
	/* Implement this */
    InitSysTick(48000); // if 48Mhz, then 48000000 ticks per 1 second, then 48000 ticks per 1 ms
    CurrentlyRunningThread = &threadControlBlocks[0];
    uint8_t nextThreadPriority = UINT8_MAX;
    uint32_t i;
    for(i = 0; i < NumberOfThreads; i++){
        if((&threadControlBlocks[i])->priority < nextThreadPriority){
            CurrentlyRunningThread = &threadControlBlocks[i];
            nextThreadPriority = CurrentlyRunningThread->priority;
        }
    }
    //SHPR3 |= PENDSV_PRI | SYSTICK_PRI;
    NVIC_SetPriority(PendSV_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
    G8RTOS_Start();
    return 0;
}


/*
 * Adds threads to G8RTOS Scheduler
 * 	- Checks if there are stil available threads to insert to scheduler
 * 	- Initializes the thread control block for the provided thread
 * 	- Initializes the stack for the provided thread to hold a "fake context"
 * 	- Sets stack tcb stack pointer to top of thread stack
 * 	- Sets up the next and previous tcb pointers in a round robin fashion
 * Param "threadToAdd": Void-Void Function to add as preemptable main thread
 * Returns: Error code for adding threads
 */
int G8RTOS_AddThread(void (*threadToAdd)(void), uint8_t priority, char * name)
{
	/* Implement this */
    int32_t PRIMASK = 0;
    PRIMASK = StartCriticalSection();

    if(NumberOfThreads == MAX_THREADS){
        bool swap = false;
        uint32_t i;
        uint32_t tempAliveIndex;
        for(i = 0; i < MAX_THREADS; i++){
            if(threadControlBlocks[i].alive){
                tempAliveIndex = i;
                break;
            }
            if(i >= (MAX_THREADS-1)){
                // error code
                return 1;
            }
        }
        for(i = 0; i < NumberOfThreads; i++){
            if(!(threadControlBlocks[i].alive)){
                swap = true;

                threadStacks[i][STACKSIZE-1] = THUMBBIT; // PSR to thumb bit set
                threadStacks[i][STACKSIZE-2] = (int32_t)threadToAdd; // PC to thread function pointer
                int j;
                for(j = 3; j <= 16; j++){
                    threadStacks[i][STACKSIZE-j] = 1; // Fake Context
                }

                threadControlBlocks[i].Stack_Pointer = &threadStacks[i][STACKSIZE-16];
                threadControlBlocks[i].Previous_tcb = &threadControlBlocks[tempAliveIndex];
                threadControlBlocks[i].Next_tcb = threadControlBlocks[tempAliveIndex].Next_tcb;
                threadControlBlocks[tempAliveIndex].Next_tcb->Previous_tcb = &threadControlBlocks[i];
                threadControlBlocks[tempAliveIndex].Next_tcb = &threadControlBlocks[i];

                threadControlBlocks[i].blocked = 0;
                threadControlBlocks[i].asleep = false;
                threadControlBlocks[i].sleep_count = 0;
                threadControlBlocks[i].priority = priority;
                strcpy(threadControlBlocks[i].threadName, name);
                threadControlBlocks[i].threadID = ((IDCounter++) << 16) | ((uint32_t)&threadControlBlocks[i]);
                threadControlBlocks[i].alive = true;

                break;
            }
        }

        EndCriticalSection(PRIMASK);

        if(!swap){
            // error code
            return 1;
        }
        return 0;
    }

    else{

        if(NumberOfThreads == 0){
            threadStacks[0][STACKSIZE-1] = THUMBBIT; // PSR to thumb bit set
            threadStacks[0][STACKSIZE-2] = (int32_t)threadToAdd; // PC to thread function pointer
            int j;
            for(j = 3; j <= 16; j++){
                threadStacks[0][STACKSIZE-j] = 1; // Fake Context
            }

            threadControlBlocks[0].Stack_Pointer = &threadStacks[0][STACKSIZE-16];
            threadControlBlocks[0].Previous_tcb = &threadControlBlocks[0];
            threadControlBlocks[0].Next_tcb = &threadControlBlocks[0];

            threadControlBlocks[0].blocked = 0;
            threadControlBlocks[0].asleep = false;
            threadControlBlocks[0].sleep_count = 0;
            threadControlBlocks[0].priority = priority;
            strcpy(threadControlBlocks[0].threadName, name);
            threadControlBlocks[0].threadID = ((IDCounter++) << 16) | ((uint32_t)&threadControlBlocks[0]);
            threadControlBlocks[0].alive = true;

            NumberOfThreads++;

            EndCriticalSection(PRIMASK);

            return 0;
        }

        uint32_t tempAliveIndex = 0;
        uint32_t tempDeadIndex = 0;
        uint32_t i;
        for(i = 0; i < MAX_THREADS; i++){
            if(threadControlBlocks[i].alive){
                tempAliveIndex = i;
                break;
            }
            if(i >= (MAX_THREADS-1)){
                // error code
                return 1;
            }
        }
        for(i = 0; i < MAX_THREADS; i++){
            if(!(threadControlBlocks[i].alive)){
                tempDeadIndex = i;
                break;
            }
            if(i >= (MAX_THREADS-1)){
                // error code
                return 1;
            }
        }

        threadStacks[tempDeadIndex][STACKSIZE-1] = THUMBBIT; // PSR to thumb bit set
        threadStacks[tempDeadIndex][STACKSIZE-2] = (int32_t)threadToAdd; // PC to thread function pointer
        int j;
        for(j = 3; j <= 16; j++){
            threadStacks[tempDeadIndex][STACKSIZE-j] = 1; // Fake Context
        }

        //tcb_t * tempPrevious = threadControlBlocks[tempAliveIndex].Previous_tcb;
        //tcb_t * tempNext = threadControlBlocks[tempAliveIndex].Next_tcb;

        threadControlBlocks[tempDeadIndex].Stack_Pointer = &threadStacks[tempDeadIndex][STACKSIZE-16];
        threadControlBlocks[tempDeadIndex].Previous_tcb = &threadControlBlocks[tempAliveIndex];
        threadControlBlocks[tempDeadIndex].Next_tcb = threadControlBlocks[tempAliveIndex].Next_tcb;
        threadControlBlocks[tempAliveIndex].Next_tcb->Previous_tcb = &threadControlBlocks[tempDeadIndex];
        threadControlBlocks[tempAliveIndex].Next_tcb = &threadControlBlocks[tempDeadIndex];

        threadControlBlocks[tempDeadIndex].blocked = 0;
        threadControlBlocks[tempDeadIndex].asleep = false;
        threadControlBlocks[tempDeadIndex].sleep_count = 0;
        threadControlBlocks[tempDeadIndex].priority = priority;
        strcpy(threadControlBlocks[tempDeadIndex].threadName, name);
        threadControlBlocks[tempDeadIndex].threadID = ((IDCounter++) << 16) | ((uint32_t)&threadControlBlocks[tempDeadIndex]);
        threadControlBlocks[tempDeadIndex].alive = true;

        NumberOfThreads++;

        EndCriticalSection(PRIMASK);

        return 0;


        /*
        threadStacks[NumberOfThreads][STACKSIZE-1] = THUMBBIT; // PSR to thumb bit set
        threadStacks[NumberOfThreads][STACKSIZE-2] = (int32_t)threadToAdd; // PC to thread function pointer
        int i;
        for(i = 3; i <= 16; i++){
            threadStacks[NumberOfThreads][STACKSIZE-i] = 1; // Fake Context
        }
        // You pass function pointers in as an argument to the AddThread function,
        // then in the AddThread function you store that pointer in the "PC" field
        // of the exception stack model (look in the Exception Entry section in the tech manual)

        if(NumberOfThreads == 0){
            threadControlBlocks[0].Stack_Pointer = &threadStacks[0][STACKSIZE-16];
            threadControlBlocks[0].Previous_tcb = &threadControlBlocks[0];
            threadControlBlocks[0].Next_tcb = &threadControlBlocks[0];
        }
        else{
            threadControlBlocks[NumberOfThreads].Stack_Pointer = &threadStacks[NumberOfThreads][STACKSIZE-16];
            threadControlBlocks[NumberOfThreads].Previous_tcb = &threadControlBlocks[NumberOfThreads-1];
            threadControlBlocks[NumberOfThreads].Next_tcb = &threadControlBlocks[0];
            threadControlBlocks[0].Previous_tcb = &threadControlBlocks[NumberOfThreads];
            threadControlBlocks[NumberOfThreads-1].Next_tcb = &threadControlBlocks[NumberOfThreads];
        }

        threadControlBlocks[NumberOfThreads].blocked = 0;
        threadControlBlocks[NumberOfThreads].asleep = false;
        threadControlBlocks[NumberOfThreads].sleep_count = 0;
        threadControlBlocks[NumberOfThreads].priority = priority;
        threadControlBlocks[NumberOfThreads].threadName = name;
        threadControlBlocks[NumberOfThreads].threadID = ((IDCounter++) << 16) | ((uint32_t)threadControlBlocks[NumberOfThreads]);
        threadControlBlocks[NumberOfThreads].alive = true;

        NumberOfThreads++;

        EndCriticalSection(PRIMASK);

        return 0;
        */

    }
}

void OS_Sleep(uint32_t duration){
    // duration in milliseconds
    CurrentlyRunningThread->sleep_count = duration + SystemTime;
    // put thread to sleep
    CurrentlyRunningThread->asleep = true;
    // start context switch
    ICSR |= ICSR_PENDSVSET;
}

int32_t G8RTOS_AddPeriodicEvent(void (*handlerToAdd)(void), uint32_t periodToAdd){
    if(NumberOfPeriodicEvents == MAX_PERIODICEVENTS){
        // error code
        return 1;
    }
    else{
        periodicEvents[NumberOfPeriodicEvents].Handler = handlerToAdd;
        periodicEvents[NumberOfPeriodicEvents].period = periodToAdd;
        periodicEvents[NumberOfPeriodicEvents].current_time = SystemTime;
        periodicEvents[NumberOfPeriodicEvents].execute_time = SystemTime + periodToAdd;

        if(NumberOfPeriodicEvents == 0){
            periodicEvents[0].Previous_pe = &periodicEvents[0];
            periodicEvents[0].Next_pe = &periodicEvents[0];
        }
        else{
            periodicEvents[NumberOfPeriodicEvents].Previous_pe = &periodicEvents[NumberOfPeriodicEvents-1];
            periodicEvents[NumberOfPeriodicEvents].Next_pe = &periodicEvents[0];
            periodicEvents[0].Previous_pe = &periodicEvents[NumberOfPeriodicEvents];
            periodicEvents[NumberOfPeriodicEvents-1].Next_pe = &periodicEvents[NumberOfPeriodicEvents];
        }

        NumberOfPeriodicEvents++;

        return 0;

    }
}

int32_t initFIFO(uint32_t indexOfFIFOs){
    if(indexOfFIFOs > FIFOSIZE){
        // error
        return 1;
    }
    else{
        int i;
        for(i = 0; i < FIFOSIZE; i++){
            FIFOs[indexOfFIFOs].Buffer[i] = 0;
        }
        FIFOs[indexOfFIFOs].Head = &(FIFOs[indexOfFIFOs].Buffer[0]);
        FIFOs[indexOfFIFOs].Tail = &(FIFOs[indexOfFIFOs].Buffer[0]);
        FIFOs[indexOfFIFOs].lostData = 0;
        FIFOs[indexOfFIFOs].currentSize = 0;
        FIFOs[indexOfFIFOs].mutex = 1;
        return 0;
    }
}

int32_t readFIFO(int32_t indexOfFIFOs){
    G8RTOS_WaitSemaphore(&(FIFOs[indexOfFIFOs].mutex));
    G8RTOS_WaitSemaphore(&(FIFOs[indexOfFIFOs].currentSize));

    // get index of head
    /*int32_t indexOfData = 0;
    int i;
    for(i = 0; i < FIFOSIZE; i++){
        if(FIFOs[indexOfFIFOs].Head == &(FIFOs[indexOfFIFOs].Buffer[i])){
            indexOfData = i;
        }
    }*/

    // read data
    int32_t dataToRead = *(FIFOs[indexOfFIFOs].Head);

    // increment head pointer
    if(FIFOs[indexOfFIFOs].Head == &(FIFOs[indexOfFIFOs].Buffer[FIFOSIZE-1])){
        FIFOs[indexOfFIFOs].Head = &(FIFOs[indexOfFIFOs].Buffer[0]);
    }
    else{
        FIFOs[indexOfFIFOs].Head++;
    }

    G8RTOS_SignalSemaphore(&(FIFOs[indexOfFIFOs].mutex));
    return dataToRead;
}

int32_t writeFIFO(int32_t indexOfFIFOs, int32_t dataToWrite){
    //G8RTOS_WaitSemaphore(&(FIFOs[indexOfFIFOs].mutex));

    // check to see if overwriting old data
    if(FIFOs[indexOfFIFOs].currentSize > FIFOSIZE){
        FIFOs[indexOfFIFOs].lostData++;
        return FIFOs[indexOfFIFOs].lostData;
    }

    // get index of tail
    /*int32_t indexOfData = 0;
    int i;
    for(i = 0; i < FIFOSIZE; i++){
        if(FIFOs[indexOfFIFOs].Tail == &(FIFOs[indexOfFIFOs].Buffer[i])){
            indexOfData = i;
        }
    }*/

    // write data
    *(FIFOs[indexOfFIFOs].Tail) = dataToWrite;

    // increment tail pointer
    if(FIFOs[indexOfFIFOs].Tail == &(FIFOs[indexOfFIFOs].Buffer[FIFOSIZE-1])){
        FIFOs[indexOfFIFOs].Tail = &(FIFOs[indexOfFIFOs].Buffer[0]);
    }
    else{
        FIFOs[indexOfFIFOs].Tail++;
    }

    //G8RTOS_SignalSemaphore(&(FIFOs[indexOfFIFOs].mutex));
    G8RTOS_SignalSemaphore(&(FIFOs[indexOfFIFOs].currentSize));
    return FIFOs[indexOfFIFOs].lostData;
}

threadId_t G8RTOS_GetThreadId(){
    return CurrentlyRunningThread->threadID;
}

sched_ErrCode_t G8RTOS_KillThread(threadId_t threadID){
    int32_t PRIMASK = 0;
    PRIMASK = StartCriticalSection();

    if(NumberOfThreads == 1){
        // error code
        EndCriticalSection(PRIMASK);
        return 1;
    }
    uint32_t i;
    for(i = 0; i < NumberOfThreads; i++){
        if(threadControlBlocks[i].threadID == threadID){
            threadControlBlocks[i].alive = false;
            tcb_t * tempPrevious = threadControlBlocks[i].Previous_tcb;
            tcb_t * tempNext = threadControlBlocks[i].Next_tcb;
            tempPrevious->Next_tcb = tempNext;
            tempNext->Previous_tcb = tempPrevious;
            NumberOfThreads--;
            EndCriticalSection(PRIMASK);
            if(&threadControlBlocks[i] == CurrentlyRunningThread){
                ICSR |= ICSR_PENDSVSET;
            }
            return 0;
        }
    }

    // error code
    EndCriticalSection(PRIMASK);
    return 1;
}

sched_ErrCode_t G8RTOS_KillSelf(){
    int32_t PRIMASK = 0;
    PRIMASK = StartCriticalSection();

    if(NumberOfThreads == 1){
        // error code
        EndCriticalSection(PRIMASK);
        return 1;
    }
    CurrentlyRunningThread->alive = false;
    tcb_t * tempPrevious = CurrentlyRunningThread->Previous_tcb;
    tcb_t * tempNext = CurrentlyRunningThread->Next_tcb;
    tempPrevious->Next_tcb = tempNext;
    tempNext->Previous_tcb = tempPrevious;
    NumberOfThreads--;

    EndCriticalSection(PRIMASK);
    ICSR |= ICSR_PENDSVSET;
    return 0;
}

sched_ErrCode_t G8RTOS_AddAPeriodicEvent(void (*AthreadToAdd)(void), uint8_t priority, IRQn_Type IRQn){
    // __NVIC_SetVector(IRQn, vector)
    // __NVIC_SetPriority(IRQn, priority)
    // __NVIC_EnableIRQ(IRQn)
    // must be > PSS_IRQn (0)
    // must be < PORT6_IRQn (40)
    if((IRQn < PSS_IRQn) || (IRQn > PORT6_IRQn)){
        // return error
        return 1;
    }
    if(priority > 6){
        // return error
        return 1;
    }
    __NVIC_SetVector(IRQn, (uint32_t)AthreadToAdd);
    __NVIC_SetPriority(IRQn, priority);
    __NVIC_EnableIRQ(IRQn);
    return 0;
}

sched_ErrCode_t G8RTOS_KillOthers(){
    int32_t PRIMASK = 0;
    PRIMASK = StartCriticalSection();

    if(NumberOfThreads == 1){
        EndCriticalSection(PRIMASK);
        return 0;
    }
    uint32_t i;
    for(i = 0; i < NumberOfThreads; i++){
        if(&threadControlBlocks[i] == CurrentlyRunningThread){
            threadControlBlocks[i].Previous_tcb = CurrentlyRunningThread;
            threadControlBlocks[i].Next_tcb = CurrentlyRunningThread;
        }
        else{
            threadControlBlocks[i].alive = false;
        }
    }

    NumberOfThreads = 1;
    EndCriticalSection(PRIMASK);
    return 0;
}

/*********************************************** Public Functions *********************************************************************/
