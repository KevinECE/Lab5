/*
 * G8RTOS_Structure.h
 *
 *  Created on: Jan 12, 2017
 *      Author: Raz Aloni
 */

#ifndef G8RTOS_STRUCTURES_H_
#define G8RTOS_STRUCTURES_H_

#include "G8RTOS.h"
#include "stdbool.h"
#include "G8RTOS_Structures.h"

/*********************************************** Data Structure Definitions ***********************************************************/


/*
 *  Thread Control Block:
 *      - Every thread has a Thread Control Block
 *      - The Thread Control Block holds information about the Thread Such as the Stack Pointer, Priority Level, and Blocked Status
 *      - For Lab 2 the TCB will only hold the Stack Pointer, next TCB and the previous TCB (for Round Robin Scheduling)
 */

/* Create tcb struct here */

struct tcb_t{
    int32_t * Stack_Pointer;
    struct tcb_t * Previous_tcb;
    struct tcb_t * Next_tcb;
    semaphore_t * blocked;
    bool asleep;
    uint32_t sleep_count;
    uint8_t priority;
    bool alive;
    char threadName[MAX_NAME_LENGTH];
    threadId_t threadID;
};
typedef struct tcb_t tcb_t;

struct pe_t{
    void (*Handler)(void);
    uint32_t period;
    uint32_t execute_time;
    uint32_t current_time;
    struct pe_t * Previous_pe;
    struct pe_t * Next_pe;
};
typedef struct pe_t pe_t;

struct fifo_t{
    int32_t Buffer[FIFOSIZE];
    int32_t * Head;
    int32_t * Tail;
    uint32_t lostData;
    semaphore_t currentSize;
    semaphore_t mutex;
};
typedef struct fifo_t fifo_t;

struct ball_t{
    uint16_t xPos;
    uint16_t yPos;
    int16_t xVel;
    int16_t yVel;
    bool alive;
    threadId_t threadID;
    uint16_t color;
};
typedef struct ball_t ball_t;

/*********************************************** Data Structure Definitions ***********************************************************/


/*********************************************** Public Variables *********************************************************************/

tcb_t * CurrentlyRunningThread;

/*********************************************** Public Variables *********************************************************************/




#endif /* G8RTOS_STRUCTURES_H_ */
