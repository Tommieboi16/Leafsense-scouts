/*
 * Led.c
 *
 *   Created on: 22 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 *
 * Implementation of the non-blocking LED pattern state machine.
 * Each pattern is described as a list of (LED state, duration) pairs.
 */

#include "Led.h"
#include "stm32f4xx_hal.h"

/* LD2 is on GPIOA pin PA5 */
#define LED_PORT        GPIOA
#define LED_PIN         GPIO_PIN_5

/* A pattern is a list of segments. A segment is: turn LED on/off, hold for N ms.
 * A zero duration marks end of list; the pattern then returns to IDLE
 */
typedef struct {
    uint8_t  on;                        // 1 = LED on, 0 = LED off
    uint16_t duration;                  // ms to hold this segment (0 = end of list)
} LedSegment_t;

/* Pattern tables*/

static const LedSegment_t pat_idle[] = {
    {1, 100},
    {0, 1900},
    {0, 0}
};

static const LedSegment_t pat_tx_event[] = {
    {1, 50}, {0, 50},
    {1, 50}, {0, 50},
    {1, 50}, {0, 200},
    {0, 0}
};

static const LedSegment_t pat_success[] = {
    {1, 100}, {0, 100},
    {1, 100}, {0, 200},
    {0, 0}
};

static const LedSegment_t pat_auth_fail[] = {
    {1, 100}, {0, 100}, {1, 100}, {0, 100},
    {1, 100}, {0, 100}, {1, 100}, {0, 100},
    {1, 100}, {0, 100}, {1, 100}, {0, 100},
    {1, 100}, {0, 100}, {1, 100}, {0, 100},
    {1, 100}, {0, 100}, {1, 100}, {0, 100},
    {0, 0}
};

static const LedSegment_t pat_eeprom_error[] = {
    {1, 3000},
    {0, 0}
};

static const LedSegment_t pat_boot[] = {
    {1, 200}, {0, 200},
    {1, 200}, {0, 200},
    {1, 200}, {0, 200},
    {0, 0}
};

/* Runtime state */
static const LedSegment_t* currentPattern   = pat_idle;
static LedPattern_t        currentPatternId = LED_PATTERN_IDLE;
static uint8_t             segmentIndex     = 0;
static uint32_t            segmentStartMs   = 0;
static uint8_t             isLooping        = 1;     // IDLE loops; others are one-shot

/*
 */

static const LedSegment_t* pattern_table(LedPattern_t p)
{
    switch (p) {
        case LED_PATTERN_IDLE:          return pat_idle;
        case LED_PATTERN_TX_EVENT:      return pat_tx_event;
        case LED_PATTERN_SUCCESS:       return pat_success;
        case LED_PATTERN_AUTH_FAIL:     return pat_auth_fail;
        case LED_PATTERN_EEPROM_ERROR:  return pat_eeprom_error;
        case LED_PATTERN_BOOT:          return pat_boot;
        default:                        return pat_idle;
    }
}

static void apply_segment(void)
{
    if (currentPattern[segmentIndex].on) {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    }
    segmentStartMs = HAL_GetTick();
}

/*
 */

void Led_Init(void)
{
    currentPatternId = LED_PATTERN_IDLE;
    currentPattern   = pat_idle;
    segmentIndex     = 0;
    isLooping        = 1;
    apply_segment();
}

void Led_SetPattern(LedPattern_t pattern)
{
    currentPatternId = pattern;
    currentPattern   = pattern_table(pattern);
    segmentIndex     = 0;
    // Only IDLE loops
    isLooping        = (pattern == LED_PATTERN_IDLE) ? 1 : 0;
    apply_segment();
}

void Led_Tick(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - segmentStartMs;

    if (elapsed < currentPattern[segmentIndex].duration) {
        return;   // still in current segment, nothing to do
    }

    // Advance to next segment
    segmentIndex++;

    // End-of-list sentinel?
    if (currentPattern[segmentIndex].duration == 0) {
        if (isLooping) {
            segmentIndex = 0;   // restart IDLE loop
        } else {
            // One-shot pattern finished; return to IDLE
            Led_SetPattern(LED_PATTERN_IDLE);
            return;
        }
    }

    apply_segment();
}
