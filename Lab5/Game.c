/*
 * Game.c
 *
 *  Created on: Apr 7, 2021
 *      Author: user
 */

#include "LCDLib.h"
#include "msp.h"
#include "driverlib.h"
#include "BSP.h"
#include "cc3100_usage.h"
#include "G8RTOS_Scheduler.h"
#include "G8RTOS_Semaphores.h"
#include "Threads.h"
#include "Game.h"

semaphore_t LCDMutex;
semaphore_t LEDMutex;

// Scaling factors for velocities of balls and players
int16_t scaleX = 0;
int16_t scaleY = 0;
int16_t scaleP0 = 1;
int16_t scaleP1 = 1;

// All the balls
Ball_t balls[MAX_NUM_OF_BALLS];

// All the players
GeneralPlayerInfo_t players[MAX_NUM_OF_PLAYERS];

// Player info
SpecificPlayerInfo_t clientPlayer;

// Game state
GameState_t gameState;

// Previous ball positions
PrevBall_t prevBalls[MAX_NUM_OF_BALLS];
// Previous player positions
PrevPlayer_t prevPlayers[MAX_NUM_OF_PLAYERS];

// Points for bottom and top players
uint8_t points_bottom = 0;
uint8_t points_top = 0;

// Status of game
uint8_t gameOver = 0;

// Button press flag for restart
bool flagButton = false;

/*********************************************** Client Threads *********************************************************************/
/*
 * Thread for client to join game
 */
void JoinGame(){
    // set specific player info, probably call getLoaclIP() to get IP
    clientPlayer = (SpecificPlayerInfo_t) {
        .IP_address   = getLocalIP(),
        .displacement = 0,
        .playerNumber = 1,
        .acknowledge  = 0,
        .ready        = 0,
        .joined       = 1
    };

    // send player info to host
    SendData((uint8_t *)&clientPlayer, clientPlayer.IP_address, sizeof(clientPlayer));

    // wait for server response

    // send acknowledge, light LED

    // Initialize board state

    //

}

/*
 * Thread that receives game state packets from host
 */
void ReceiveDataFromHost()
{
//    int32_t retVal = ReceiveData();

    // Continually receive data until retVal > 0 (meaning valid data has been read)
}

/*
 * Thread that sends UDP packets to host
 */
void SendDataToHost()
{
    // send player info to host
    SendData((uint8_t *)&clientPlayer, clientPlayer.IP_address, sizeof(clientPlayer));
    // sleep for 2ms
    OS_Sleep(2);
}

/*
 * Thread to read client's joystick
 */
void ReadJoystickClient();

/*
 * End of game for the client
 */
void EndOfGameClient();

/*********************************************** Client Threads *********************************************************************/

/*********************************************** Host Threads *********************************************************************/
/*
 * Thread for the host to create a game
 */
void CreateGame(){
    // Initialize the players
    int i;
    for(i = 0; i < MAX_NUM_OF_PLAYERS; i++){
        players[i].color = LCD_WHITE;
        players[i].currentCenter = PADDLE_X_CENTER;
        if(i == 0){
            // bottom
            players[i].position = BOTTOM;
        }
        if(i == 1){
            // top
            players[i].position = TOP;
        }
        prevPlayers[i].Center = players[i].currentCenter;
    }

    // Establish connection with client - TODO - EDITED BY KEVIN
    establishConnectionWithAP();


    // Initialize the board
    G8RTOS_WaitSemaphore(&LCDMutex);
    // Clear Screen
    LCD_Clear(LCD_BLACK);
    // Draw arena, which consists of 4 rectangles as 4 sides.
    LCD_DrawRectangle(ARENA_MIN_X, ARENA_MAX_X, ARENA_MIN_Y, ARENA_MIN_Y, LCD_WHITE);
    LCD_DrawRectangle(ARENA_MIN_X, ARENA_MAX_X, ARENA_MAX_Y, ARENA_MAX_Y, LCD_WHITE);
    LCD_DrawRectangle(ARENA_MIN_X, ARENA_MIN_X, ARENA_MIN_Y, ARENA_MAX_Y, LCD_WHITE);
    LCD_DrawRectangle(ARENA_MAX_X, ARENA_MAX_X, ARENA_MIN_Y, ARENA_MAX_Y, LCD_WHITE);
    // Draw players
    LCD_DrawRectangle(PADDLE_X_CENTER - PADDLE_LEN_D2, PADDLE_X_CENTER + PADDLE_LEN_D2, ARENA_MIN_Y, TOP_PADDLE_EDGE, LCD_WHITE);
    LCD_DrawRectangle(PADDLE_X_CENTER - PADDLE_LEN_D2, PADDLE_X_CENTER + PADDLE_LEN_D2, BOTTOM_PADDLE_EDGE, ARENA_MAX_Y, LCD_WHITE);
    G8RTOS_SignalSemaphore(&LCDMutex);
    // Add all threads
    G8RTOS_AddThread(GenerateBall, 4, "gen ball");
    G8RTOS_AddThread(DrawObjects, 4, "draw obj");
    //G8RTOS_AddThread(ReadJoystickHost, 4, "joystick");
    //G8RTOS_AddThread(SendDataToClient, 4, "send data");
    //G8RTOS_AddThread(ReceiveDataFromClient, 4, "get data");
    G8RTOS_AddThread(MoveLEDs, 5, "LEDs");
    G8RTOS_AddThread(Idle, 6, "idle");
    G8RTOS_KillSelf();
}


/*
 * Thread that sends game state to client
 */
void SendDataToClient();

/*
 * Thread that receives UDP packets from client
 */
void ReceiveDataFromClient();

/*
 * Generate Ball thread
 */
void GenerateBall(){
    while(1){
        // Add a ball based on the number of balls
        // See if we reach the max number of balls
        bool isFull = true;
        uint32_t i;
        for(i = 0; i < MAX_NUM_OF_BALLS; i++){
            if(!(balls[i].alive)){
                isFull = false;
                break;
            }
        }
        if(!(isFull)){
            // Add ball and sleep a certain amount of time proportional to the current number of balls
            G8RTOS_AddThread(MoveBall, 4, "ball thread");
            OS_Sleep((i + 1) * 5000); // can be changed later
        }
    }
}

/*
 * Thread to read host's joystick
 */
void ReadJoystickHost();

/*
 * Thread to move a single ball
 */
void MoveBall(){
    // Find a dead ball to make it alive
    int i;
    for(i = 0; i < MAX_NUM_OF_BALLS; i++){
        if(!(balls[i].alive)){
            // can be randomized later
            balls[i].currentCenterX = MAX_SCREEN_X / 2;
            balls[i].currentCenterY = MAX_SCREEN_Y / 2;
            balls[i].color = LCD_WHITE;
            balls[i].alive = true;
            break;
        }
    }
    // Initialize velocities for this ball, can randomize later
    int16_t veloX = 1;
    int16_t veloY = 1;
    // infinite loop for this ball's movement
    while(1){
        // First check to see if bouncing off arena side walls
        if(((balls[i].currentCenterX + veloX) >= ARENA_MAX_X) || ((balls[i].currentCenterX + veloX) <= ARENA_MIN_X)){
            veloX = veloX * (-1);
        }
        // Next check to see if bouncing off paddles for bottom and top players
        uint8_t collide_bottom = checkCollision(&balls[i], &players[0]);
        uint8_t collide_top = checkCollision(&balls[i], &players[1]);
        if(collide_bottom == 1){
            veloY = veloY * (-1);
            balls[i].color = LCD_BLUE;
        }
        if(collide_top == 1){
            veloY = veloY * (-1);
            balls[i].color = LCD_RED;
        }
        // Move the ball
        balls[i].currentCenterX += veloX * scaleX;
        balls[i].currentCenterY += veloY * scaleY;
        // Check to see if ball passes arena top and bottom walls and check to see if game ends
        if(balls[i].currentCenterX <= ARENA_MIN_Y){
            points_bottom++;
            // Arbitrary point goal
            if(points_bottom >= 8){
                gameOver = 1;
            }
            G8RTOS_KillSelf();
        }
        if(balls[i].currentCenterX >= ARENA_MAX_Y){
            points_top++;
            if(points_top >= 8){
                gameOver = 1;
            }
            G8RTOS_KillSelf();
        }
        // Sleep for 35 ms
        OS_Sleep(35);
    }
}

/*
 * End of game for the host
 */
void EndOfGameHost(){
    // Wait for all semaphores to be released, although need to make sure no deadlocks so maybe need to change later
    G8RTOS_WaitSemaphore(&LCDMutex);
    G8RTOS_WaitSemaphore(&LEDMutex);
    // Kill all other threads
    G8RTOS_KillOthers();
    // Re-initialize all semaphores
    G8RTOS_SignalSemaphore(&LCDMutex);
    G8RTOS_SignalSemaphore(&LEDMutex);
    G8RTOS_InitSemaphore(&LCDMutex, 1);
    G8RTOS_InitSemaphore(&LEDMutex, 1);
    // Clear screen with winner's color
    if(points_bottom >= points_top){
        LCD_Clear(LCD_BLUE);
    }
    else{
        LCD_Clear(LCD_RED);
    }
    // Print message to wait for restart
    uint8_t str[] = "Wait for Restart";
    LCD_Text(MAX_SCREEN_X / 2, MAX_SCREEN_Y / 2, str, LCD_WHITE);
    // Create aperiodic thread that waits for restart from host - TODO
    // G8RTOS_AddAPeriodicEvent(LCDTap, 4, PORT4_IRQn);
    // Decide to maybe just add the thread in the beginning and we can just enable the interrupt here
    flagButton = false;
    P4->IFG &= ~BIT4;
    P4->IE |= BIT4;
    __NVIC_EnableIRQ(PORT4_IRQn);
    // Wait for button to set a global flag and then disable the interrupt
    while(!flagButton);
    __NVIC_DisableIRQ(PORT4_IRQn);
    P4->IE &= ~BIT4;
    // Send notification to client - TODO - probably done in CreateGame() instead

    // Re-initialize
    G8RTOS_AddThread(CreateGame, 4, "create");
    // Kill self
    G8RTOS_KillSelf();
}
/*********************************************** Host Threads *********************************************************************/





void UpdateBallOnScreen(PrevBall_t * previousBall, Ball_t * currentBall, uint16_t outColor){
    // if the previous ball's position is 0, then this ball has just been initialized
    if(previousBall->CenterX == 0){
        LCD_DrawRectangle(currentBall->currentCenterX - BALL_SIZE_D2,
                          currentBall->currentCenterX + BALL_SIZE_D2,
                          currentBall->currentCenterY - BALL_SIZE_D2,
                          currentBall->currentCenterY + BALL_SIZE_D2,
                          outColor);
    }
    // else we just clear previous position and update to new position
    else{
        LCD_DrawRectangle(previousBall->CenterX - BALL_SIZE_D2,
                          previousBall->CenterX + BALL_SIZE_D2,
                          previousBall->CenterY - BALL_SIZE_D2,
                          previousBall->CenterY + BALL_SIZE_D2,
                          LCD_BLACK);
        LCD_DrawRectangle(currentBall->currentCenterX - BALL_SIZE_D2,
                          currentBall->currentCenterX + BALL_SIZE_D2,
                          currentBall->currentCenterY - BALL_SIZE_D2,
                          currentBall->currentCenterY + BALL_SIZE_D2,
                          outColor);
    }
}

void UpdatePlayerOnScreen(PrevPlayer_t * prevPlayerIn, GeneralPlayerInfo_t * outPlayer){
    // check to see which player, then set up corresponding values for the Y upper and lower bounds
    uint8_t upperBound = ARENA_MIN_Y;
    uint8_t lowerBound = TOP_PADDLE_EDGE;
    if(outPlayer->position == BOTTOM){
        upperBound = BOTTOM_PADDLE_EDGE;
        lowerBound = ARENA_MAX_Y;
    }
    // Compare the old and new center positions, keep all the overlapping squares and update the remaining ones
    // if move to the right
    if(outPlayer->currentCenter > prevPlayerIn->Center){
        // calculate overlapping bounds
        // one problem with this implementation is that the paddle MUST NOT move too far in one update cycle so there is no overlap
        int16_t leftBound = outPlayer->currentCenter - PADDLE_LEN_D2;
        int16_t rightBound = prevPlayerIn->Center + PADDLE_LEN_D2;
        // clear the tail
        LCD_DrawRectangle(prevPlayerIn->Center - PADDLE_LEN_D2,
                          leftBound,
                          upperBound,
                          lowerBound,
                          LCD_BLACK);
        // update the head
        LCD_DrawRectangle(rightBound,
                          outPlayer->currentCenter + PADDLE_LEN_D2,
                          upperBound,
                          lowerBound,
                          outPlayer->color);
    }
    // else move to the left
    else if(outPlayer->currentCenter < prevPlayerIn->Center){
        // calculate overlapping bounds
        // one problem with this implementation is that the paddle MUST NOT move too far in one update cycle so there is no overlap
        int16_t leftBound = prevPlayerIn->Center - PADDLE_LEN_D2;
        int16_t rightBound = outPlayer->currentCenter + PADDLE_LEN_D2;
        // clear the tail
        LCD_DrawRectangle(rightBound,
                          prevPlayerIn->Center + PADDLE_LEN_D2,
                          upperBound,
                          lowerBound,
                          LCD_BLACK);
        // update the head
        LCD_DrawRectangle(outPlayer->currentCenter - PADDLE_LEN_D2,
                          leftBound,
                          upperBound,
                          lowerBound,
                          outPlayer->color);
    }
}

void DrawPlayer(GeneralPlayerInfo_t * player){
    if(player->position == TOP){
        // currentCenter is just the x-position since y will always stay the same
        LCD_DrawRectangle(player->currentCenter - PADDLE_LEN_D2,
                          player->currentCenter + PADDLE_LEN_D2,
                          TOP_PLAYER_CENTER_Y - PADDLE_WID_D2,
                          TOP_PLAYER_CENTER_Y + PADDLE_WID_D2,
                          player->color);
    }
    else{
        LCD_DrawRectangle(player->currentCenter - PADDLE_LEN_D2,
                          player->currentCenter + PADDLE_LEN_D2,
                          BOTTOM_PLAYER_CENTER_Y - PADDLE_WID_D2,
                          BOTTOM_PLAYER_CENTER_Y + PADDLE_WID_D2,
                          player->color);
    }
}

/*********************************************** Common Threads *********************************************************************/
/*
 * Idle thread
 */
void IdleThread()
{
    while(1);
}

/*
 * Thread to draw all the objects in the game
 */
void DrawObjects(){
    while(1){
        // iterates through all the alive balls
        int i;
        for(i = 0; i < MAX_NUM_OF_BALLS; i++){
            if(balls[i].alive){
                // call the function to update the ball's drawing
                G8RTOS_WaitSemaphore(&LCDMutex);
                UpdateBallOnScreen(&prevBalls[i], &balls[i], LCD_WHITE); // white for now, will add color later
                G8RTOS_SignalSemaphore(&LCDMutex);
                // update previous ball's position
                prevBalls[i].CenterX = balls[i].currentCenterX;
                prevBalls[i].CenterY = balls[i].currentCenterY;
            }
        }
        // players[0] is bottom and players[1] is top
        // call the functions to update the players' drawings
        G8RTOS_WaitSemaphore(&LCDMutex);
        UpdatePlayerOnScreen(&prevPlayers[0], &players[0]);
        UpdatePlayerOnScreen(&prevPlayers[1], &players[1]);
        G8RTOS_SignalSemaphore(&LCDMutex);
        // update the previous players' positions
        prevPlayers[0].Center = players[0].currentCenter;
        prevPlayers[1].Center = players[1].currentCenter;
        // sleep for 20 ms
        OS_Sleep(20);
    }
};

/*
 * Thread to update LEDs based on score
 */
void MoveLEDs(){
    while(1){
        // Calculate the values to send to the LEDs
        uint16_t blueLED = 0x0000;
        uint16_t redLED = 0x0000;
        int i;
        for(i = 16; i > (16 - points_bottom); i--){
            blueLED += 0x0001 << (i - 1);
        }
        for(i = 16; i > (16 - points_top); i--){
            redLED += 0x0001 << (i - 1);
        }
        G8RTOS_WaitSemaphore(&LEDMutex);
        LP3943_LedModeSet(BLUE, blueLED);
        LP3943_LedModeSet(RED, redLED);
        G8RTOS_SignalSemaphore(&LEDMutex);
        // may change later
        OS_Sleep(10);
    }
}

/*********************************************** Common Threads *********************************************************************/


/*********************************************** Public Functions *********************************************************************/
/*
 * Returns either Host or Client depending on button press
 */
playerType GetPlayerRole();

/*
 * Draw players given center X center coordinate
 */
void DrawPlayer(GeneralPlayerInfo_t * player);

/*
 * Updates player's paddle based on current and new center
 */
void UpdatePlayerOnScreen(PrevPlayer_t * prevPlayerIn, GeneralPlayerInfo_t * outPlayer);

/*
 * Function updates ball position on screen
 */
void UpdateBallOnScreen(PrevBall_t * previousBall, Ball_t * currentBall, uint16_t outColor);

/*
 * Initializes and prints initial game state
 */
void InitBoardState();

// check for collision
uint8_t checkCollision(Ball_t * ball, GeneralPlayerInfo_t * player){
    int32_t widthA = BALL_SIZE;
    int32_t widthB = PADDLE_LEN;
    int32_t heightA = BALL_SIZE;
    int32_t heightB = PADDLE_WID;
    int32_t centerXA = (int32_t)(ball->currentCenterX);
    int32_t centerYA = (int32_t)(ball->currentCenterY);
    int32_t centerXB = (int32_t)(player->currentCenter);
    int32_t centerYB = (player->position == TOP) ? TOP_PLAYER_CENTER_Y : BOTTOM_PLAYER_CENTER_Y;
    // Below is just the Minkowski Algorithm for Collision
    int32_t w = 0.5 * (widthA + widthB);
    int32_t h = 0.5 * (heightA + heightB);
    int32_t dx = centerXA - centerXB;
    int32_t dy = centerYA - centerYB;
    int32_t dx_abs = (dx >= 0) ? dx : (dx * (-1));
    int32_t dy_abs = (dy >= 0) ? dy : (dy * (-1));
    if((dx_abs <= w) && (dy_abs <= h)){
        // Collision happened
        int32_t wy = w * dy;
        int32_t hx = h * dx;
        if(wy > hx){
            if(wy > (hx * (-1))){
                // Collision at top
                return 1;
            }
            else{
                // Collision at left
                return 2;
            }
        }
        else{
            if(wy > (hx * (-1))){
                // Collision at right
                return 3;
            }
            else{
                // Collision at bottom
                return 4;
            }
        }
    }
    // No collision
    return 0;
}

// Aperiodic interrupt
void buttonPress(){
    flagButton = true;
}

/*********************************************** Public Functions *********************************************************************/
