/*
 * Led.h
 *
 *   Created on: 22 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 *
 * Non-blocking LED pattern state machine for LeafSense Scouts.
 * Uses LD2 (GPIOA pin PA5) on the Nucleo-F411RE. All timing is derived
 * from HAL_GetTick(). The application calls Led_Tick() in its main loop
 * and calls Led_SetPattern() to request a new pattern.
 */

#ifndef INC_LED_H_
#define INC_LED_H_

#include "stdint.h"

typedef enum {
    LED_PATTERN_IDLE,           // slow heartbeat
    LED_PATTERN_TX_EVENT,       // rapid triple flash
    LED_PATTERN_SUCCESS,        // double flash
    LED_PATTERN_AUTH_FAIL,      // 5Hz for 2s
    LED_PATTERN_EEPROM_ERROR,   // solid for 3s
    LED_PATTERN_BOOT            // triple slow flash on boot
} LedPattern_t;

/* Initialise the LED state machine. Call once after HAL startup. */
void Led_Init(void);

/* Request a new pattern. Some patterns (TX_EVENT, SUCCESS, AUTH_FAIL,
 * EEPROM_ERROR, BOOT) run once then automatically return to IDLE. */
void Led_SetPattern(LedPattern_t pattern);

/* Advance the state machine. Call every iteration of main loop.
 */
void Led_Tick(void);

#endif /* INC_LED_H_ */
