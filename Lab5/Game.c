Skip to content
Search or jump to…

Pull requests
Issues
Marketplace
Explore

@KevinECE
KevinECE
/
Lab5
1
00
Code
Issues
Pull requests
Actions
Projects
Wiki
Security
Insights
Settings
Lab5/Game.c
@x15000177
x15000177 bug fix
Latest commit 23aad4b 4 hours ago
 History
 1 contributor
920 lines (867 sloc)  30.7 KB

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
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

semaphore_t LCDMutex;
semaphore_t LEDMutex;
semaphore_t sensorMutex;
semaphore_t ballMutex;
semaphore_t dataMutex;


int16_t scaleX = 1;                                     // Scaling factors for velocities of balls
int16_t scaleY = 1;
Ball_t balls[MAX_NUM_OF_BALLS];                         // All the balls
GeneralPlayerInfo_t players[MAX_NUM_OF_PLAYERS];        // All the players
// set specific player info, probably call getLoaclIP() to get IP
SpecificPlayerInfo_t clientPlayer;                      // Player info
GameState_t gameState;                                  // Game state
PrevBall_t prevBalls[MAX_NUM_OF_BALLS];                 // Previous ball positions
PrevPlayer_t prevPlayers[MAX_NUM_OF_PLAYERS];           // Previous player positions
uint8_t points_bottom = 0;                              // Points for bottom and top players
uint8_t points_top = 0;
uint16_t blueLED = 0x0000;
uint16_t redLED = 0x0000;
uint8_t gameOver = 0;                                   // Status of game
bool flagButton = false;                                // Button press flag for restart

/*********************************************** Client Threads *********************************************************************/
/*
 * Thread for client to join game
 */
void JoinGame(){
    // Set initial SpecificPlayerInfo_tstrict attributes (you can get the IP address by calling getLocalIP()
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
    // Initialize the points
    gameOver = 0;
    points_top = 0;
    points_bottom = 0;
    redLED = 0;
    blueLED = 0;
    // Initialize the player info
    uint16_t LEDScores[2] = {blueLED, redLED};
    uint8_t overallScores[2] = {points_bottom, points_top};
    clientPlayer = (SpecificPlayerInfo_t) {
        .IP_address   = getLocalIP(),
        .displacement = 0,
        .playerNumber = 1,
        .acknowledge  = false,
        .ready        = false,
        .joined       = false
    };
    gameState = (GameState_t) {
        .player = (SpecificPlayerInfo_t) {
            .IP_address   = 0,
            .displacement = 0,
            .playerNumber = 0,
            .acknowledge  = false,
            .ready        = false,
            .joined       = false
        },
        .numberOfBalls = 0,
        .winner = false,
        .gameDone = false,
    };
    memcpy(&gameState.players, &players, sizeof(gameState.players));
    memcpy(&gameState.balls, &balls, sizeof(gameState.players));
    memcpy(&gameState.LEDScores, &LEDScores, sizeof(LEDScores));
    memcpy(&gameState.overallScores, &overallScores, sizeof(overallScores));

    // send player info to host
    clientPlayer.joined = true;
    G8RTOS_WaitSemaphore(&dataMutex);
    SendData((uint8_t *)&clientPlayer, HOST_IP_ADDR, sizeof(clientPlayer));
    G8RTOS_SignalSemaphore(&dataMutex);

    // wait for server response with ready = true
    int32_t retVal = -1;
    while(!(retVal >= 0) || (gameState.player.ready == false))
    {
        G8RTOS_WaitSemaphore(&dataMutex);
        retVal = ReceiveData((uint8_t *)&gameState, sizeof(gameState));
        G8RTOS_SignalSemaphore(&dataMutex);
    }
    clientPlayer.ready = gameState.player.ready;

    // send acknowledge, light LED
    clientPlayer.acknowledge = true;
    G8RTOS_WaitSemaphore(&dataMutex);
    SendData((uint8_t *)&clientPlayer, HOST_IP_ADDR, sizeof(clientPlayer));
    G8RTOS_SignalSemaphore(&dataMutex);
    //LP3943_LedModeSet(GREEN, 0);

    // Initialize the board
    InitBoardState();

    // Add all threads
    G8RTOS_AddThread(DrawObjects, 4, "draw obj");
    G8RTOS_AddThread(ReadJoystickClient, 4, "joystick");
    G8RTOS_AddThread(SendDataToHost, 4, "send data");
    G8RTOS_AddThread(ReceiveDataFromHost, 4, "get data");
    G8RTOS_AddThread(MoveLEDs, 5, "LEDs");
    G8RTOS_AddThread(Idle, 6, "idle");
    G8RTOS_KillSelf();
}

/*
 * Thread that receives game state packets from host
 */
void ReceiveDataFromHost()
{
    while(1)
    {
        int32_t retVal = -1;

        // Continually receive data until retVal > 0 (meaning valid data has been read)
        while(!(retVal >= 0))
        {
            G8RTOS_WaitSemaphore(&dataMutex);
            retVal = ReceiveData((uint8_t *)&gameState, sizeof(gameState));
            G8RTOS_SignalSemaphore(&dataMutex);
            OS_Sleep(1);
        }

        // Empty the package
        memcpy(&players, &gameState.players, sizeof(players));
        memcpy(&balls, &gameState.balls, sizeof(balls));
        gameOver = gameState.gameDone;
        blueLED = gameState.LEDScores[0];
        redLED = gameState.LEDScores[1];
        points_bottom = gameState.overallScores[0];
        points_top = gameState.overallScores[1];

        // Check if game is done
        if(gameOver){
           G8RTOS_AddThread(EndOfGameClient, 3, "end");
        }
        // Sleep 5 ms
        OS_Sleep(5);
    }
}

/*
 * Thread that sends UDP packets to host
 */
void SendDataToHost()
{
    while(1)
    {
        // send player info to host
        G8RTOS_WaitSemaphore(&dataMutex);
        SendData((uint8_t *)&clientPlayer, HOST_IP_ADDR, sizeof(clientPlayer));
        G8RTOS_SignalSemaphore(&dataMutex);
        // sleep for 2ms
        OS_Sleep(2);
    }
}

/*
 * Thread to read client's joystick
 */
void ReadJoystickClient(){
    while(1){
        // Read joystick coordinates
        int16_t joyX = 0;
        int16_t joyY = 0;
        GetJoystickCoordinates(&joyX, &joyY);
        // Divide by scale, can be adjusted later, also probably need to add bias later
        // Also need to make sure the difference bewteen the two joyX values are not greater than
        joyX = joyX / 2000;
        G8RTOS_WaitSemaphore(&sensorMutex);
        //Add joystick to top player purposes
        if((players[1].currentCenter -= joyX) < HORIZ_CENTER_MIN_PL){
            players[1].currentCenter = HORIZ_CENTER_MIN_PL;
        }
        else if((players[1].currentCenter -= joyX) > HORIZ_CENTER_MAX_PL){
            players[1].currentCenter = HORIZ_CENTER_MAX_PL;
        }
        else{
            players[1].currentCenter -= joyX;
        }
        // Add to displacement
        clientPlayer.displacement = joyX;
        G8RTOS_SignalSemaphore(&sensorMutex);
        // Sleep for 10 ms
        OS_Sleep(10);
    }
}

/*
 * End of game for the client
 */
void EndOfGameClient(){
    // Wait for all semaphores to be released, although need to make sure no deadlocks so maybe need to change later
    G8RTOS_WaitSemaphore(&LCDMutex);
    G8RTOS_WaitSemaphore(&LEDMutex);
    G8RTOS_WaitSemaphore(&sensorMutex);
    G8RTOS_WaitSemaphore(&ballMutex);
    G8RTOS_WaitSemaphore(&dataMutex);
    // Kill all other threads
    G8RTOS_KillOthers();
    // Kill all balls
    int i;
    for(i = 0; i < MAX_NUM_OF_BALLS; i++){
        balls[i].alive = false;
    }
    // Re-initialize all semaphores
    G8RTOS_SignalSemaphore(&LCDMutex);
    G8RTOS_SignalSemaphore(&LEDMutex);
    G8RTOS_SignalSemaphore(&sensorMutex);
    G8RTOS_SignalSemaphore(&ballMutex);
    G8RTOS_SignalSemaphore(&dataMutex);
    G8RTOS_InitSemaphore(&LCDMutex, 1);
    G8RTOS_InitSemaphore(&LEDMutex, 1);
    G8RTOS_InitSemaphore(&sensorMutex, 1);
    G8RTOS_InitSemaphore(&ballMutex, 1);
    G8RTOS_InitSemaphore(&dataMutex, 1);
    // Clear screen with winner's color
    if(points_bottom >= points_top){
        LCD_Clear(LCD_BLUE);
    }
    else{
        LCD_Clear(LCD_RED);
    }
    // Print message to wait for restart
    uint8_t str[] = "Wait for Restart...";
    LCD_Text(MAX_SCREEN_X / 4, MAX_SCREEN_Y / 2, str, LCD_WHITE);
    // Wait for host to restart game
    // wait for server response with ready = true
    gameState.player.ready = false;
    int32_t retVal = -1;
    while(!(retVal >= 0) || (gameState.player.ready == false))
    {
        G8RTOS_WaitSemaphore(&dataMutex);
        retVal = ReceiveData((uint8_t *)&gameState, sizeof(gameState));
        G8RTOS_SignalSemaphore(&dataMutex);
    }
    // Add all threads back
    G8RTOS_AddThread(JoinGame, 4, "join");
    // Kill self
    G8RTOS_KillSelf();
}
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
    // Initialize the points
    gameOver = 0;
    points_top = 0;
    points_bottom = 0;
    redLED = 0;
    blueLED = 0;
    // Initialize the player info
    uint16_t LEDScores[2] = {blueLED, redLED};
    uint8_t overallScores[2] = {points_bottom, points_top};
    clientPlayer = (SpecificPlayerInfo_t) {
        .IP_address   = 0,
        .displacement = 0,
        .playerNumber = 1,
        .acknowledge  = false,
        .ready        = false,
        .joined       = false
    };
    gameState = (GameState_t) {
        .player = (SpecificPlayerInfo_t) {
            .IP_address   = getLocalIP(),
            .displacement = 0,
            .playerNumber = 0,
            .acknowledge  = false,
            .ready        = false,
            .joined       = false
        },
        .numberOfBalls = 0,
        .winner = false,
        .gameDone = false,
    };
    memcpy(&gameState.players, &players, sizeof(gameState.players));
    memcpy(&gameState.balls, &balls, sizeof(gameState.players));
    memcpy(&gameState.LEDScores, &LEDScores, sizeof(LEDScores));
    memcpy(&gameState.overallScores, &overallScores, sizeof(overallScores));

    // Establish connection with client
    int32_t retVal = -1;

    // Wait for client response with joined = true
    while(!(retVal >= 0) || (clientPlayer.joined == false))
    {
        G8RTOS_WaitSemaphore(&dataMutex);
        retVal = ReceiveData((uint8_t *)&clientPlayer, sizeof(clientPlayer));
        G8RTOS_SignalSemaphore(&dataMutex);
    }
    gameState.player.joined = clientPlayer.joined;
    retVal = -1;

    // Send ready to client
    gameState.player.ready = true;
    G8RTOS_WaitSemaphore(&dataMutex);
    SendData((uint8_t *)&gameState, HOST_IP_ADDR, sizeof(gameState));
    G8RTOS_SignalSemaphore(&dataMutex);

    // Wait for acknowledge
    while(!(retVal >= 0) || (clientPlayer.acknowledge == false))
    {
        G8RTOS_WaitSemaphore(&dataMutex);
        retVal = ReceiveData((uint8_t *)&clientPlayer, sizeof(clientPlayer));
        G8RTOS_SignalSemaphore(&dataMutex);
    }
    gameState.player.acknowledge = clientPlayer.acknowledge;

    // Initialize the board
    InitBoardState();

    // Add all threads
    G8RTOS_AddThread(GenerateBall, 4, "gen ball");
    G8RTOS_AddThread(DrawObjects, 4, "draw obj");
    G8RTOS_AddThread(ReadJoystickHost, 4, "joystick");
    G8RTOS_AddThread(SendDataToClient, 4, "send data");
    G8RTOS_AddThread(ReceiveDataFromClient, 4, "get data");
    G8RTOS_AddThread(MoveLEDs, 5, "LEDs");
    G8RTOS_AddThread(Idle, 6, "idle");
    G8RTOS_KillSelf();
}

/*
 * Thread that sends game state to client
 */
void SendDataToClient()
{
    while(1)
    {
        G8RTOS_WaitSemaphore(&dataMutex);

        // Fill packet for client
        uint16_t numberOfBalls = 0;
        int i;
        for(i = 0; i < MAX_NUM_OF_BALLS; i++){
            if(balls[i].alive){
                numberOfBalls++;
            }
        }
        bool winner = (points_bottom >= points_top) ? false : true;
        bool gameDone = (bool)gameOver;
        uint16_t LEDScores[2] = {blueLED, redLED};
        uint8_t overallScores[2] = {points_bottom, points_top};

        gameState = (GameState_t) {
            .player = (SpecificPlayerInfo_t) {
                .IP_address   = getLocalIP(),
                .displacement = 0,
                .playerNumber = 0,
                .acknowledge  = true,
                .ready        = true,
                .joined       = true
            },
            .numberOfBalls = numberOfBalls,
            .winner = winner,
            .gameDone = gameDone,
        };
        memcpy(&gameState.players, &players, sizeof(gameState.players));
        memcpy(&gameState.balls, &balls, sizeof(gameState.players));
        memcpy(&gameState.LEDScores, &LEDScores, sizeof(LEDScores));
        memcpy(&gameState.overallScores, &overallScores, sizeof(overallScores));

        // Send packet
        SendData((uint8_t *)&gameState, HOST_IP_ADDR, sizeof(gameState));

        G8RTOS_SignalSemaphore(&dataMutex);

        // Check if game is done
        if(gameOver)
        {
            G8RTOS_AddThread(EndOfGameHost, 3, "End game");
        }

        OS_Sleep(5);
    }
}

/*
 * Thread that receives UDP packets from client
 */
void ReceiveDataFromClient()
{
    while(1)
    {
        int32_t retVal = -1;

        // Continually receive data until retVal >= 0 (meaning valid data has been read)
        while(!(retVal >= 0))
        {
            G8RTOS_WaitSemaphore(&dataMutex);
            retVal = ReceiveData((uint8_t *)&clientPlayer, sizeof(clientPlayer));
            G8RTOS_SignalSemaphore(&dataMutex);
            OS_Sleep(1);
        }

        // Update the client's position
        int16_t joyX = clientPlayer.displacement;
        if((players[1].currentCenter -= joyX) < HORIZ_CENTER_MIN_PL){
            players[1].currentCenter = HORIZ_CENTER_MIN_PL;
        }
        else if((players[1].currentCenter -= joyX) > HORIZ_CENTER_MAX_PL){
            players[1].currentCenter = HORIZ_CENTER_MAX_PL;
        }
        else{
            players[1].currentCenter -= joyX;
        }

        // Sleep 2 ms
        OS_Sleep(2);

    }
}

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
            OS_Sleep((i + 1) * 1000); // can be changed later
        }
    }
}

/*
 * Thread to read host's joystick
 */
void ReadJoystickHost(){
    while(1){
        // Read joystick coordinates
        int16_t joyX = 0;
        int16_t joyY = 0;
        GetJoystickCoordinates(&joyX, &joyY);
        // Divide by scale, can be adjusted later, also probably need to add bias later
        // Also need to make sure the difference bewteen the two joyX values are not greater than
        joyX = joyX / 2000;
        // Sleep for 10 ms
        OS_Sleep(10);
        // Add joystick to bottom player
        G8RTOS_WaitSemaphore(&sensorMutex);
        if((players[0].currentCenter -= joyX) < HORIZ_CENTER_MIN_PL){
            players[0].currentCenter = HORIZ_CENTER_MIN_PL;
        }
        else if((players[0].currentCenter -= joyX) > HORIZ_CENTER_MAX_PL){
            players[0].currentCenter = HORIZ_CENTER_MAX_PL;
        }
        else{
            players[0].currentCenter -= joyX;
        }
        // Add joystick to top player for testing purposes
        /*if((players[1].currentCenter -= joyX) < HORIZ_CENTER_MIN_PL){
            players[1].currentCenter = HORIZ_CENTER_MIN_PL;
        }
        else if((players[1].currentCenter -= joyX) > HORIZ_CENTER_MAX_PL){
            players[1].currentCenter = HORIZ_CENTER_MAX_PL;
        }
        else{
            players[1].currentCenter -= joyX;
        }*/
        G8RTOS_SignalSemaphore(&sensorMutex);
    }
}


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
    int16_t veloX = genRandVelo();
    int16_t veloY = genRandVelo();
    // infinite loop for this ball's movement
    while(1){
        G8RTOS_WaitSemaphore(&ballMutex);
        // First check to see if bouncing off arena side walls
        if(((balls[i].currentCenterX + veloX) >= ARENA_MAX_X) || ((balls[i].currentCenterX + veloX) <= ARENA_MIN_X)){
            veloX = veloX * (-1);
        }
        // Next check to see if bouncing off paddles for bottom and top players
        uint8_t collide_bottom = checkCollision(&balls[i], &players[0]);
        uint8_t collide_top = checkCollision(&balls[i], &players[1]);
        if(collide_bottom != 0){
            veloY = veloY * (-1);
            balls[i].color = LCD_BLUE;
        }
        if(collide_top != 0){
            veloY = veloY * (-1);
            balls[i].color = LCD_RED;
        }
        // Check to see if ball passes arena top and bottom walls and check to see if game ends
        bool killBall = false;
        if(balls[i].currentCenterY <= ARENA_MIN_Y){
            killBall = true;
            points_bottom++;
        }
        if(balls[i].currentCenterY >= ARENA_MAX_Y){
            killBall = true;
            points_top++;
        }
        // Calculate LED values
        int j;
        int16_t tempBlue = 0;
        int16_t tempRed = 0;
        for(j = 0; j < points_bottom; j++){
            tempBlue += 0x0001 << j;
        }
        for(j = 16; j > (16 - points_top); j--){
            tempRed += 0x0001 << (j - 1);
        }
        blueLED = tempBlue;
        redLED = tempRed;
        // if ball touches the top or bottom
        if(killBall){
            balls[i].alive = false;
            G8RTOS_WaitSemaphore(&LCDMutex);
            LCD_DrawRectangle(balls[i].currentCenterX - BALL_SIZE_D2,
                              balls[i].currentCenterX + BALL_SIZE_D2,
                              balls[i].currentCenterY - BALL_SIZE_D2,
                              balls[i].currentCenterY + BALL_SIZE_D2,
                              LCD_BLACK);
            G8RTOS_SignalSemaphore(&LCDMutex);
            // Check to see if game ends
            if((points_top >= 8) || (points_bottom >= 8)){
                gameOver = 1;
            }
            G8RTOS_SignalSemaphore(&ballMutex);
            G8RTOS_KillSelf();
        }
        // Move the ball
        balls[i].currentCenterX += veloX * scaleX;
        balls[i].currentCenterY += veloY * scaleY;
        G8RTOS_SignalSemaphore(&ballMutex);
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
    G8RTOS_WaitSemaphore(&sensorMutex);
    G8RTOS_WaitSemaphore(&ballMutex);
    G8RTOS_WaitSemaphore(&dataMutex);
    // Kill all other threads
    G8RTOS_KillOthers();
    // Kill all balls
    int i;
    for(i = 0; i < MAX_NUM_OF_BALLS; i++){
        balls[i].alive = false;
    }
    // Re-initialize all semaphores
    G8RTOS_SignalSemaphore(&LCDMutex);
    G8RTOS_SignalSemaphore(&LEDMutex);
    G8RTOS_SignalSemaphore(&sensorMutex);
    G8RTOS_SignalSemaphore(&ballMutex);
    G8RTOS_SignalSemaphore(&dataMutex);
    G8RTOS_InitSemaphore(&LCDMutex, 1);
    G8RTOS_InitSemaphore(&LEDMutex, 1);
    G8RTOS_InitSemaphore(&sensorMutex, 1);
    G8RTOS_InitSemaphore(&ballMutex, 1);
    G8RTOS_InitSemaphore(&dataMutex, 1);
    // Clear screen with winner's color
    if(points_bottom >= points_top){
        LCD_Clear(LCD_BLUE);
    }
    else{
        LCD_Clear(LCD_RED);
    }
    // Print message to wait for restart
    uint8_t str[] = "Wait for Restart...";
    LCD_Text(MAX_SCREEN_X / 4, MAX_SCREEN_Y / 2, str, LCD_WHITE);
    // Change the current status to be not ready
    gameState.player.ready = false;
    // Create aperiodic thread that waits for restart from host
    flagButton = false;
    P4->IFG &= ~BIT4;
    P4->IE |= BIT4;
    G8RTOS_AddAPeriodicEvent(buttonPress, 4, PORT4_IRQn);
    // Wait for button to set a global flag
    while(!flagButton);
    // Send notification to client - TODO - probably done in CreateGame() instead
    gameState.player.ready = true;
    G8RTOS_WaitSemaphore(&dataMutex);
    SendData((uint8_t *)&gameState, HOST_IP_ADDR, sizeof(gameState));
    G8RTOS_SignalSemaphore(&dataMutex);
    // Re-initialize
    G8RTOS_AddThread(CreateGame, 4, "create");
    // Kill self
    G8RTOS_KillSelf();
}

/*********************************************** Host Threads *********************************************************************/

/*********************************************** Common Threads *********************************************************************/
/*
 * Idle thread
 */
/*void Idle()
{
    while(1);
}*/

/*
 * Thread to draw all the objects in the game
 */
void DrawObjects(){
    while(1){
        // Temporary: check if game ends, will move to another thread later
        if(gameOver){
            G8RTOS_AddThread(EndOfGameHost, 3, "end");
            OS_Sleep(1);
        }
        // iterates through all the alive balls
        int i;
        for(i = 0; i < MAX_NUM_OF_BALLS; i++){
            if(balls[i].alive){
                // call the function to update the ball's drawing
                G8RTOS_WaitSemaphore(&ballMutex);
                G8RTOS_WaitSemaphore(&LCDMutex);
                UpdateBallOnScreen(&prevBalls[i], &balls[i], balls[i].color);
                G8RTOS_SignalSemaphore(&LCDMutex);
                // update previous ball's position
                prevBalls[i].CenterX = balls[i].currentCenterX;
                prevBalls[i].CenterY = balls[i].currentCenterY;
                G8RTOS_SignalSemaphore(&ballMutex);
            }
        }
        // players[0] is bottom and players[1] is top
        // call the functions to update the players' drawings
        G8RTOS_WaitSemaphore(&sensorMutex);
        G8RTOS_WaitSemaphore(&LCDMutex);
        UpdatePlayerOnScreen(&prevPlayers[0], &players[0]);
        UpdatePlayerOnScreen(&prevPlayers[1], &players[1]);
        G8RTOS_SignalSemaphore(&LCDMutex);
        // update the previous players' positions
        prevPlayers[0].Center = players[0].currentCenter;
        prevPlayers[1].Center = players[1].currentCenter;
        G8RTOS_SignalSemaphore(&sensorMutex);
        // sleep for 20 ms
        OS_Sleep(20);
    }
}

/*
 * Thread to update LEDs based on score
 */
void MoveLEDs(){
    LP3943_LedModeSet(BLUE, 0);
    LP3943_LedModeSet(RED, 0);
    LP3943_LedModeSet(GREEN,0);
    while(1){
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
 * Draw players given center X center coordinate
 */
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

/*
 * Updates player's paddle based on current and new center
 */
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

/*
 * Function updates ball position on screen
 */
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

/*
 * Initializes and prints initial game state
 */
void InitBoardState(){

    G8RTOS_WaitSemaphore(&LCDMutex);
    // Clear Screen
    LCD_Clear(LCD_BLACK);
    // Draw arena, which consists of 4 rectangles as 4 sides.
    //LCD_DrawRectangle(ARENA_MIN_X, ARENA_MAX_X, ARENA_MIN_Y, ARENA_MIN_Y, LCD_WHITE);
    //LCD_DrawRectangle(ARENA_MIN_X, ARENA_MAX_X, ARENA_MAX_Y, ARENA_MAX_Y, LCD_WHITE);
    LCD_DrawRectangle(ARENA_MIN_X - 5, ARENA_MIN_X - 5, ARENA_MIN_Y, ARENA_MAX_Y, LCD_WHITE);
    LCD_DrawRectangle(ARENA_MAX_X + 5, ARENA_MAX_X + 5, ARENA_MIN_Y, ARENA_MAX_Y, LCD_WHITE);
    // Draw players
    LCD_DrawRectangle(160 - 32, 160 + 32, 0, 4, LCD_WHITE);
    LCD_DrawRectangle(160 - 32, 160 + 32, 236, 240, LCD_WHITE);
    G8RTOS_SignalSemaphore(&LCDMutex);
}

// check for collision
uint8_t checkCollision(Ball_t * ball, GeneralPlayerInfo_t * player){
    int32_t widthA = BALL_SIZE + WIGGLE_ROOM;
    int32_t widthB = PADDLE_LEN;
    int32_t heightA = BALL_SIZE + WIGGLE_ROOM;
    int32_t heightB = PADDLE_WID;
    int32_t centerXA = (int32_t)(ball->currentCenterX);
    int32_t centerYA = (int32_t)(ball->currentCenterY);
    int32_t centerXB = (int32_t)(player->currentCenter);
    int32_t centerYB = (player->position == TOP) ? TOP_PLAYER_CENTER_Y : BOTTOM_PLAYER_CENTER_Y;
    // Below is just the Minkowski Algorithm for Collision
    int32_t w = (widthA + widthB) / 2;
    int32_t h = (heightA + heightB) / 2;
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
    __NVIC_DisableIRQ(PORT4_IRQn);
    P4->IE &= ~BIT4;
    flagButton = true;
}

// get random velocity
int16_t genRandVelo(){
    //srand(time(0));
    int16_t velo = (int16_t)(rand()%16 - 8);
    if(velo == 0){
        velo = 16;
    }
    return velo;
}
© 2021 GitHub, Inc.
Terms
Privacy
Security
Status
Docs
Contact GitHub
Pricing
API
Training
Blog
About
Loading complete
