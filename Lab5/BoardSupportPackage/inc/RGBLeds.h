/*
 * RGBLeds.h
 *
 *  Created on: Jan 18, 2021
 *      Author: user
 */

#ifndef RGBLEDS_H_
#define RGBLEDS_H_

/* Enums for RGB LEDs*/
typedef enum device{
    BLUE = 0,
    GREEN = 1,
    RED = 2,
} device;

/*
 * LP3943_ColorSet
 * This function will set the frequencies and the PWM duty cycle
 * for each register of the specified unit.
 */
static void LP3943_ColorSet(uint32_t unit, uint32_t PWM_DATA);

/*
 * LP3943_LedModeSet
 * This function will set each of the LEDs to the desired operating
 * mode. The operating modes are on, off, PWM1 and PWM2.
 */
void LP3943_LedModeSet(uint32_t unit, uint16_t LED_DATA);

/*
 * Performs necessary initializations for RGB LEDs
 */
void init_RGBLEDs();

#endif /* RGBLEDS_H_ */
