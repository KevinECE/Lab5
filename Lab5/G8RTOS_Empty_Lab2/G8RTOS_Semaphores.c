/*
 * G8RTOS_Semaphores.c
 */

/*********************************************** Dependencies and Externs *************************************************************/

#include <stdint.h>
#include "msp.h"
#include "G8RTOS_Semaphores.h"
#include "G8RTOS_CriticalSection.h"
#include "G8RTOS_Scheduler.h"
#include "G8RTOS_Structures.h"

extern tcb_t * CurrentlyRunningThread;

#define ICSR (*((volatile unsigned int*)(0xe000ed04)))
#define ICSR_PENDSVSET (1 << 28)

/*********************************************** Dependencies and Externs *************************************************************/


/*********************************************** Public Functions *********************************************************************/

/*
 * Initializes a semaphore to a given value
 * Param "s": Pointer to semaphore
 * Param "value": Value to initialize semaphore to
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_InitSemaphore(semaphore_t *s, int32_t value)
{
	/* Implement this */
    int32_t PRIMASK = 0;
    PRIMASK = StartCriticalSection();
    *s = value;
    EndCriticalSection(PRIMASK);
}

/*
 * Waits for a semaphore to be available (value greater than 0)
 * 	- Decrements semaphore when available
 * 	- Spinlocks to wait for semaphore
 * Param "s": Pointer to semaphore to wait on
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_WaitSemaphore(semaphore_t *s)
{
	/* Implement this */
    //int i;
    //for(i = 0; i < 100000; i++);

    int32_t PRIMASK = 0;
    PRIMASK = StartCriticalSection();

    /*while(*s == 0){
        EndCriticalSection(PRIMASK);

        int j;
        for(j = 0; j < 100000; j++);

        PRIMASK = StartCriticalSection();
    }*/

    // decrement semaphore
    (*s)--;

    // block thread if semaphore unavailable
    if((*s) < 0){
        // block thread
        CurrentlyRunningThread->blocked = s;
        // start context switch to another thread
        ICSR |= ICSR_PENDSVSET;
    }

    EndCriticalSection(PRIMASK);

    //for(i = 0; i < 100000; i++);
}

/*
 * Signals the completion of the usage of a semaphore
 * 	- Increments the semaphore value by 1
 * Param "s": Pointer to semaphore to be signalled
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_SignalSemaphore(semaphore_t *s)
{
	/* Implement this */
    int32_t PRIMASK = 0;
    PRIMASK = StartCriticalSection();

    // increment semaphore
    (*s)++;

    // go through list and unblock first blocked thread with the same semaphore
    if((*s) <= 0){
        tcb_t * pt = CurrentlyRunningThread->Next_tcb;
        while(pt->blocked != s){
            pt = pt->Next_tcb;
        }
        pt->blocked = 0;
    }

    EndCriticalSection(PRIMASK);
}

/*********************************************** Public Functions *********************************************************************/


