/*
 * FieldMap.h
 *
 *   Created on: 21 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 *
 * Structured EEPROM storage for the LeafSense field disease map.
 * Only used by the Base Scout. Abstracts the 24LC64 byte layout into
 * a clean "log this disease event for this zone" API.
 *
 * Storage layout (see FieldMap.c for byte-exact design):
 *   - Page 0 (bytes 0..31): Config block with magic byte, event counter, UID
 *   - Pages 1..255: One per zone, 32 bytes each, with latest reading,
 *                   cumulative stats, and a 13-entry ring buffer.
 */

#ifndef INC_FIELDMAP_H_
#define INC_FIELDMAP_H_

#include "stdint.h"

/* Public API */

/* Called once at Base Scout startup.
 */
uint16_t FieldMap_Init(void);

/* Log a disease event for a given zone.
 * Updates both the zone's per-page record and the global config counter.
 * @param zone        1..255 (zone 0 is reserved, rejected)
 * @param disease     disease class code
 * @param severity    0..255
 * @param srcMacLow   lowest byte of the sender's 64-bit MAC
 * @return 1 on success, 0 on failure */
uint8_t FieldMap_LogEvent(uint8_t zone, uint8_t disease, uint8_t severity, uint8_t srcMacLow);

/* Read the latest recorded event for a zone into a compact struct.
 * Useful for audit */
typedef struct {
    uint8_t  latest_disease;
    uint8_t  latest_severity;
    uint16_t event_count;
    uint8_t  max_severity;
} FieldMap_ZoneSummary_t;

uint8_t FieldMap_GetZoneSummary(uint8_t zone, FieldMap_ZoneSummary_t* out);

/* Dump the entire field map over huart2 (USB serial) for demonstration.
 */
void FieldMap_DumpToUART(void);

#endif /* INC_FIELDMAP_H_ */
