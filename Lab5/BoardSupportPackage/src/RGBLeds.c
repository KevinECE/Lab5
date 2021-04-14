/*
 * RGBLeds.c
 *
 *  Created on: Jan 18, 2021
 *      Author: user
 */

#include "msp.h"
#include <driverlib.h>
#include "RGBLeds.h"

/*static void LP3943_ColorSet(uint32_t unit, uint32_t PWM_DATA){

}*/

void LP3943_LedModeSet(uint32_t unit, uint16_t LED_DATA){

    if(unit == RED){
        UCB2I2CSA = 0x62; // slave address
    }
    else if(unit == GREEN){
        UCB2I2CSA = 0x61;
    }
    else{
        UCB2I2CSA = 0x60;  // BLUE
    }

    // Separate data for 4 registers
    uint8_t LS0_temp = (uint8_t)((LED_DATA >> 0) & 0xF);
    uint8_t LS1_temp = (uint8_t)((LED_DATA >> 4) & 0xF);
    uint8_t LS2_temp = (uint8_t)((LED_DATA >> 8) & 0xF);
    uint8_t LS3_temp = (uint8_t)((LED_DATA >> 12) & 0xF);

    // Add the filler zeros
    int i;
    uint8_t LS0_DATA = 0;
    uint8_t LS1_DATA = 0;
    uint8_t LS2_DATA = 0;
    uint8_t LS3_DATA = 0;
    for(i = 1; i <= 128; i = i*2){
        LS0_DATA += (LS0_temp & i)*(LS0_temp & i);
        LS1_DATA += (LS1_temp & i)*(LS1_temp & i);
        LS2_DATA += (LS2_temp & i)*(LS2_temp & i);
        LS3_DATA += (LS3_temp & i)*(LS3_temp & i);
    }

    UCB2CTLW0 |= UCTXSTT; // START
    while(UCB2CTLW0 & UCTXSTT); // POLL for START
    // while(!(UCB2IFG & UCTXIFG0)); // POLL

    UCB2TXBUF = 0x16; // 0b00010110, 0b110 for LS0 address, and 0b10000 for auto increment
    while(!(UCB2IFG & UCTXIFG0)); // POLL

    UCB2TXBUF = LS0_DATA; // send LS0 data for LEDs 0-3
    while(!(UCB2IFG & UCTXIFG0)); // POLL

    UCB2TXBUF = LS1_DATA; // send LS1 data for LEDs 4-7
    while(!(UCB2IFG & UCTXIFG0)); // POLL

    UCB2TXBUF = LS2_DATA; // send LS2 data for LEDs 8-11
    while(!(UCB2IFG & UCTXIFG0)); // POLL

    UCB2TXBUF = LS3_DATA; // send LS3 data for LEDs 12-15
    while(!(UCB2IFG & UCTXIFG0)); // POLL

    UCB2CTLW0 |= UCTXSTP; // STOP
}

void init_RGBLEDs(){

    uint16_t UNIT_OFF = 0x0000;

    // Software reset enable
    UCB2CTLW0 = UCSWRST;

    // Initialize I2C master
    // Set as master, I2C mode, Clock sync, SMCLK source, Transmitter
    UCB2CTLW0 |= EUSCI_B_CTLW0_MST | EUSCI_B_CTLW0_MODE_3 | EUSCI_B_CTLW0_SYNC | EUSCI_B_CTLW0_SSEL__SMCLK | EUSCI_B_CTLW0_TR;

    // Set the Fclk as 400kHz
    // Presumes that the SMCLK is selected as source and Fsmclk is 12 MHz
    UCB2BRW = 30;

    // In conjunction with the next line, this sets the pins as I2C mode.
    // (Table found on p160 of SLAS826E)
    // Set P3.6 as UCB2_SDA and 3.7 as UCB2_SLC
    P3SEL0 |= 0xC0; // 0b11000000
    P3SEL1 &= 0x3F; // 0b00111111

    // Bitwise anding of all bits except UCSWRST.
    UCB2CTLW0 &= ~UCSWRST;

    LP3943_LedModeSet(RED, UNIT_OFF);
    LP3943_LedModeSet(GREEN, UNIT_OFF);
    LP3943_LedModeSet(BLUE, UNIT_OFF);

}
