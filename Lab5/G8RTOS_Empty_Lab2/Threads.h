/*
 * Threads.h
 *
 *  Created on: Feb 10, 2021
 *      Author: user
 */

#ifndef G8RTOS_EMPTY_LAB2_THREADS_H_
#define G8RTOS_EMPTY_LAB2_THREADS_H_

#define JOYSTICKFIFO 0
#define TEMPFIFO 1
#define LIGHTFIFO 2

void ReadAccel();
void LCDTap();
void WaitForTap();
void Idle();
void Ball();
uint16_t genRandColor();



#endif /* G8RTOS_EMPTY_LAB2_THREADS_H_ */
