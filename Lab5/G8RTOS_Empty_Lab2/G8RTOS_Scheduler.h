/*
 * G8RTOS_Scheduler.h
 */

#ifndef G8RTOS_SCHEDULER_H_
#define G8RTOS_SCHEDULER_H_

/*********************************************** Sizes and Limits *********************************************************************/
#define MAX_THREADS 23
#define STACKSIZE 512
#define OSINT_PRIORITY 7

#define MAX_PERIODICEVENTS 6
#define FIFOSIZE 16
#define MAX_FIFOS 4

#define MAX_NAME_LENGTH 16
/*********************************************** Sizes and Limits *********************************************************************/

/*********************************************** Public Variables *********************************************************************/

/* Holds the current time for the whole System */
extern uint32_t SystemTime;

/*********************************************** Public Variables *********************************************************************/


/*********************************************** Public Functions *********************************************************************/

typedef uint32_t threadId_t;
typedef int32_t sched_ErrCode_t;

/*
 * Initializes variables and hardware for G8RTOS usage
 */
void G8RTOS_Init();

/*
 * Starts G8RTOS Scheduler
 * 	- Initializes Systick Timer
 * 	- Sets Context to first thread
 * Returns: Error Code for starting scheduler. This will only return if the scheduler fails
 */
int32_t G8RTOS_Launch();

/*
 * Adds threads to G8RTOS Scheduler
 * 	- Checks if there are stil available threads to insert to scheduler
 * 	- Initializes the thread control block for the provided thread
 * 	- Initializes the stack for the provided thread
 * 	- Sets up the next and previous tcb pointers in a round robin fashion
 * Param "threadToAdd": Void-Void Function to add as preemptable main thread
 * Returns: Error code for adding threads
 */
int32_t G8RTOS_AddThread(void (*threadToAdd)(void), uint8_t priority, char * name);

void OS_Sleep(uint32_t duration);

int32_t G8RTOS_AddPeriodicEvent(void (*handlerToAdd)(void), uint32_t periodToAdd);

int32_t initFIFO(uint32_t indexOfFIFOs);

int32_t readFIFO(int32_t indexOfFIFOs);

int32_t writeFIFO(int32_t indexOfFIFOs, int32_t dataToWrite);

threadId_t G8RTOS_GetThreadId();

sched_ErrCode_t G8RTOS_KillThread(threadId_t threadID);

sched_ErrCode_t G8RTOS_KillSelf();

sched_ErrCode_t G8RTOS_AddAPeriodicEvent(void (*AthreadToAdd)(void), uint8_t priority, IRQn_Type IRQn);

sched_ErrCode_t G8RTOS_KillOthers();

/*********************************************** Public Functions *********************************************************************/

#endif /* G8RTOS_SCHEDULER_H_ */
