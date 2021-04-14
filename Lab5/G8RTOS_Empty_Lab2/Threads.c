/*
 * Threads.c
 *
 *  Created on: Feb 10, 2021
 *      Author: user
 */

#include <stdint.h>
#include "msp.h"
#include <driverlib.h>
#include "BSP.h"
#include "G8RTOS_Semaphores.h"
#include "Threads.h"
#include "G8RTOS_Structures.h"
#include "G8RTOS_Scheduler.h"
#include <stdio.h>
#include "LCDLib.h"
#include <stdlib.h>
#include <time.h>

semaphore_t sem_sensor;
semaphore_t sem_LED;
semaphore_t sem_count;
semaphore_t sensorMutex;
semaphore_t LEDMutex;
semaphore_t LCDMutex;

uint8_t count[4] = {0, 0, 0, 0};
int32_t lightData[FIFOSIZE];
int32_t indexOFLight = 0;
int32_t tempData = 0;
int32_t decayAve = 0;

uint8_t toggle[3] = {0, 0, 0};
uint8_t smallRMS = 0;

int16_t accelX = 0;
int16_t accelY = 0;
uint8_t flagTP = 0;
ball_t ballArray[20];



void ReadAccel(){
    while(1){
        G8RTOS_WaitSemaphore(&sensorMutex);
        while(bmi160_read_accel_x(&accelX));
        while(bmi160_read_accel_y(&accelY));
        G8RTOS_SignalSemaphore(&sensorMutex);
        OS_Sleep(10);
    }
}

void LCDTap(){
    __NVIC_DisableIRQ(PORT4_IRQn);
    P4->IE &= ~BIT0;
    flagTP = 1;
    //G8RTOS_AddThread(WaitForTap, 5, "wait thread");
}

void WaitForTap(){
    while(1){
        if(flagTP){
            Point tempP = TP_ReadXY();
            bool deleted = false;
            int i;
            for(i = 0; i < 20; i++){
                uint16_t diffX = (tempP.x > ballArray[i].xPos) ? tempP.x - ballArray[i].xPos : ballArray[i].xPos - tempP.x;
                uint16_t diffY = (tempP.y > ballArray[i].yPos) ? tempP.y - ballArray[i].yPos : ballArray[i].yPos - tempP.y;
                if((diffX <= 25) && (diffY <= 25)){
                    //G8RTOS_WaitSemaphore(&LCDMutex);
                    deleted = true;
                    ballArray[i].alive = false;
                    //LCD_DrawRectangle(ballArray[i].xPos-2, ballArray[i].xPos+2, ballArray[i].yPos-2, ballArray[i].yPos+2, LCD_BLACK);
                    //G8RTOS_SignalSemaphore(&LCDMutex);
                    //G8RTOS_KillThread(ballArray[i].threadID);
                }
            }
            if(!deleted){
                writeFIFO(0, tempP.x); // 0 for x
                writeFIFO(1, tempP.y); // 1 for y
                G8RTOS_AddThread(Ball, 5, "ball thread");
            }
            OS_Sleep(500);
            P4->IFG &= ~BIT0; // must clear IFG flag, can also read PxIV to clear IFG
            P4->IE |= BIT0;
            __NVIC_EnableIRQ(PORT4_IRQn);
            flagTP = 0;
            //G8RTOS_KillSelf();
        }
    }
}

void Idle(){
    while(1);
}

void Ball(){
    int i;
    for(i = 0; i < 20; i++){
        if(!(ballArray[i].alive)){
            ballArray[i].alive = true;
            ballArray[i].xPos = readFIFO(0);
            ballArray[i].yPos = readFIFO(1);
            ballArray[i].xVel = accelX/(320*3);
            ballArray[i].yVel = accelY/(240*3);
            ballArray[i].color = genRandColor();
            ballArray[i].threadID = G8RTOS_GetThreadId();
            break;
        }
    }
    while(1){
        G8RTOS_WaitSemaphore(&LCDMutex);
        LCD_DrawRectangle(ballArray[i].xPos-2, ballArray[i].xPos+2, ballArray[i].yPos-2, ballArray[i].yPos+2, LCD_BLACK);
        ballArray[i].xVel = accelX/(320*3);
        ballArray[i].yVel = accelY/(240*3);
        if((ballArray[i].xVel > 0) && (((int16_t)ballArray[i].xPos + ballArray[i].xVel) >= MAX_SCREEN_X)){
            ballArray[i].xPos = (uint16_t)((int16_t)ballArray[i].xPos + ballArray[i].xVel - MAX_SCREEN_X);
        }
        else if((ballArray[i].xVel < 0) && (((int16_t)ballArray[i].xPos + ballArray[i].xVel) < 0)){
            ballArray[i].xPos = (uint16_t)((int16_t)ballArray[i].xPos + ballArray[i].xVel + MAX_SCREEN_X);
        }
        else{
            ballArray[i].xPos += ballArray[i].xVel;
        }
        if((ballArray[i].yVel > 0) && (((int16_t)ballArray[i].yPos + ballArray[i].yVel) >= MAX_SCREEN_Y)){
            ballArray[i].yPos = (uint16_t)((int16_t)ballArray[i].yPos + ballArray[i].yVel - MAX_SCREEN_Y);
        }
        else if((ballArray[i].yVel < 0) && (((int16_t)ballArray[i].yPos + ballArray[i].yVel) < 0)){
            ballArray[i].yPos = (uint16_t)((int16_t)ballArray[i].yPos + ballArray[i].yVel + MAX_SCREEN_Y);
        }
        else{
            ballArray[i].yPos += ballArray[i].yVel;
        }
        LCD_DrawRectangle(ballArray[i].xPos-2, ballArray[i].xPos+2, ballArray[i].yPos-2, ballArray[i].yPos+2, ballArray[i].color);
        G8RTOS_SignalSemaphore(&LCDMutex);
        OS_Sleep(30);
        if(!(ballArray[i].alive)){
            G8RTOS_WaitSemaphore(&LCDMutex);
            LCD_DrawRectangle(ballArray[i].xPos-2, ballArray[i].xPos+2, ballArray[i].yPos-2, ballArray[i].yPos+2, LCD_BLACK);
            G8RTOS_SignalSemaphore(&LCDMutex);
            G8RTOS_KillSelf();
        }
    }
}

uint16_t genRandColor(){
    srand(time(0));
    int caseNum = rand()%13;
    uint16_t color;
    switch(caseNum){
        case 1:
            //white
            color = 0xFFFF;
            break;
        case 2:
            color = 0xDFE4;
            break;
        case 3:
            //blue
            color = 0x0197;
            break;
        case 4:
            //red
            color = 0xF800;
            break;
        case 5:
            //magenta
            color = 0xF81F;
            break;
        case 6:
            //green
            color = 0x07E0;
            break;
        case 7:
            //cyan
            color = 0x7FFF;
            break;
        case 8:
            //yelow
            color = 0xFFE0;
            break;
        case 9:
            //gray
            color = 0x2104;
            break;
        case 10:
            color = 0xF11F;
            break;
        case 11:
            color = 0xFD20;
            break;
        case 12:
            //gray
            color = 0xFDBA;
            break;
        default:
            color = 0xFFFF;
            break;
    }
    return color;
}
