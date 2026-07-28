/*
 * FieldMap.c
 *
 *   Created on: 21 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 *
 * Implementation of the Base Scout's field disease map storage.
 * Uses the EEPROM library  (Lab 3).
 */

#include "Fieldmap.h"
#include "EEPROM.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef  hi2c1;          // for EEPROM access
extern UART_HandleTypeDef huart2;         // for DumpToUART

/* Layout constants */
#define FM_PAGE_SIZE          32
#define FM_TOTAL_PAGES        256
#define FM_CONFIG_PAGE        0

#define FM_MAGIC              0xA6        // "LeafSense" marker
#define FM_SCHEMA_VERSION     0x01

/* Offsets within the 32-byte config page (page 0) */
#define CFG_OFF_MAGIC         0
#define CFG_OFF_SCHEMA        1
#define CFG_OFF_EVENT_COUNT   2    // 16-bit LE
#define CFG_OFF_LAST_ZONE     4
#define CFG_OFF_UID_START     12   // 12 bytes of UID

/* Offsets within each zone page */
#define Z_OFF_LATEST_DISEASE  0
#define Z_OFF_LATEST_SEVERITY 1
#define Z_OFF_EVENT_COUNT     2    // 16-bit LE
#define Z_OFF_MAX_SEVERITY    4
#define Z_OFF_SRC_MAC_LOW     5
#define Z_OFF_RING_BUFFER     6    // 26 bytes = 13 × 2-byte entries

/*
 */

/* Write a single byte to any EEPROM memory address.
 */
static void eeprom_write_byte(uint16_t memAddr, uint8_t value)
{
    uint16_t page   = memAddr / FM_PAGE_SIZE;
    uint16_t offset = memAddr % FM_PAGE_SIZE;
    EEPROM_Write(page, offset, &value, 1);
}

static uint8_t eeprom_read_byte(uint16_t memAddr)
{
    uint16_t page   = memAddr / FM_PAGE_SIZE;
    uint16_t offset = memAddr % FM_PAGE_SIZE;
    uint8_t  value  = 0;
    EEPROM_Read(page, offset, &value, 1);
    return value;
}

/* Read a 16-bit little-endian value from EEPROM */
static uint16_t eeprom_read_u16_le(uint16_t memAddr)
{
    uint8_t lo = eeprom_read_byte(memAddr);
    uint8_t hi = eeprom_read_byte(memAddr + 1);
    return ((uint16_t)hi << 8) | lo;
}

/* Write a 16-bit little-endian value to EEPROM */
static void eeprom_write_u16_le(uint16_t memAddr, uint16_t value)
{
    eeprom_write_byte(memAddr,     (uint8_t)(value & 0xFF));
    eeprom_write_byte(memAddr + 1, (uint8_t)((value >> 8) & 0xFF));
}

/*
 */



/* Perform a first-time format of the field map. */
static void format_map(void)
{
    // Clear the config page first and verify
    uint8_t config[FM_PAGE_SIZE];
    memset(config, 0xFF, FM_PAGE_SIZE);
    config[CFG_OFF_MAGIC]         = FM_MAGIC;
    config[CFG_OFF_SCHEMA]        = FM_SCHEMA_VERSION;
    config[CFG_OFF_EVENT_COUNT]   = 0x00;
    config[CFG_OFF_EVENT_COUNT+1] = 0x00;
    config[CFG_OFF_LAST_ZONE]     = 0x00;

    HAL_Delay(10);                                       // ensure WP is low and settled
    EEPROM_Write(FM_CONFIG_PAGE, 0, config, FM_PAGE_SIZE);
    HAL_Delay(10);                                       // ensure write cycle completed

    // Now clear all the zone pages
    uint8_t blank[FM_PAGE_SIZE];
    memset(blank, 0xFF, FM_PAGE_SIZE);
    for (uint16_t p = 1; p < FM_TOTAL_PAGES; p++) {
        EEPROM_Write(p, 0, blank, FM_PAGE_SIZE);
    }
}

/*
 */

uint16_t FieldMap_Init(void)
{
    HAL_Delay(10);                                        // give I²C and WP line time to stabilise after boot

    uint8_t page0[FM_PAGE_SIZE];
    EEPROM_Read(0, 0, page0, FM_PAGE_SIZE);

    char buf[96];
    int n = snprintf(buf, sizeof(buf),
                     "\r\n[BASE] Page0 raw: %02X %02X %02X %02X %02X %02X %02X %02X",
                     page0[0], page0[1], page0[2], page0[3],
                     page0[4], page0[5], page0[6], page0[7]);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, n, HAL_MAX_DELAY);

    uint8_t magic = page0[0];

    if (magic != FM_MAGIC) {
        n = snprintf(buf, sizeof(buf),
                     "\r\n[BASE] Magic mismatch (got 0x%02X, want 0x%02X). Formatting...",
                     magic, FM_MAGIC);
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, n, HAL_MAX_DELAY);

        format_map();

        // Re-read and confirm
        HAL_Delay(10);
        EEPROM_Read(0, 0, page0, FM_PAGE_SIZE);
        n = snprintf(buf, sizeof(buf),
                     "\r\n[BASE] Post-format page0: %02X %02X %02X %02X %02X %02X %02X %02X",
                     page0[0], page0[1], page0[2], page0[3],
                     page0[4], page0[5], page0[6], page0[7]);
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, n, HAL_MAX_DELAY);

        return 0;
    }

    uint16_t eventCount = ((uint16_t)page0[CFG_OFF_EVENT_COUNT+1] << 8) | page0[CFG_OFF_EVENT_COUNT];
    return eventCount;
}

/*
 */

uint8_t FieldMap_LogEvent(uint8_t zone, uint8_t disease, uint8_t severity, uint8_t srcMacLow)
{
    // Zone 0 reserved
    if (zone == 0) {
        return 0;
    }

    // Each zone lives in its own page (page = zone number)
    uint16_t zonePage = zone;

    // Update latest disease and severity
    EEPROM_Write(zonePage, Z_OFF_LATEST_DISEASE,  &disease,  1);
    EEPROM_Write(zonePage, Z_OFF_LATEST_SEVERITY, &severity, 1);

    // Update event counter for this zone
    uint16_t zoneAddrBase = zone * FM_PAGE_SIZE;
    uint16_t zoneCount    = eeprom_read_u16_le(zoneAddrBase + Z_OFF_EVENT_COUNT);
    zoneCount++;
    eeprom_write_u16_le(zoneAddrBase + Z_OFF_EVENT_COUNT, zoneCount);

    // Update max severity if this one beats the record
    uint8_t currentMax = eeprom_read_byte(zoneAddrBase + Z_OFF_MAX_SEVERITY);
    if (currentMax == 0xFF || severity > currentMax) {
        EEPROM_Write(zonePage, Z_OFF_MAX_SEVERITY, &severity, 1);
    }

    // Record sender identity
    EEPROM_Write(zonePage, Z_OFF_SRC_MAC_LOW, &srcMacLow, 1);

    // Ring buffer update
    uint8_t slot = (uint8_t)((zoneCount - 1) % 13);
    uint16_t ringOffset = Z_OFF_RING_BUFFER + slot * 2;
    EEPROM_Write(zonePage, ringOffset,     &disease,  1);
    EEPROM_Write(zonePage, ringOffset + 1, &severity, 1);

    // Update global config page: event counter and last zone
    uint16_t globalCount = eeprom_read_u16_le(CFG_OFF_EVENT_COUNT);
    globalCount++;
    eeprom_write_u16_le(CFG_OFF_EVENT_COUNT, globalCount);
    eeprom_write_byte(CFG_OFF_LAST_ZONE, zone);

    return 1;
}

/*
 */

uint8_t FieldMap_GetZoneSummary(uint8_t zone, FieldMap_ZoneSummary_t* out)
{
    if (zone == 0 || out == NULL) return 0;

    uint16_t zoneAddrBase = zone * FM_PAGE_SIZE;

    out->latest_disease  = eeprom_read_byte(zoneAddrBase + Z_OFF_LATEST_DISEASE);
    out->latest_severity = eeprom_read_byte(zoneAddrBase + Z_OFF_LATEST_SEVERITY);
    out->event_count     = eeprom_read_u16_le(zoneAddrBase + Z_OFF_EVENT_COUNT);
    out->max_severity    = eeprom_read_byte(zoneAddrBase + Z_OFF_MAX_SEVERITY);

    return 1;
}

/*
 */

void FieldMap_DumpToUART(void)
{
    char buf[128];
    int n;

    uint16_t totalEvents = eeprom_read_u16_le(CFG_OFF_EVENT_COUNT);
    n = snprintf(buf, sizeof(buf),
                 "\r\n=== LeafSense Field Map ===\r\n"
                 "Total events logged: %u\r\n"
                 "Zone | Disease | Sev | Count | Max\r\n"
                 "-----+---------+-----+-------+----\r\n",
                 totalEvents);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, n, HAL_MAX_DELAY);

    for (uint16_t zone = 1; zone <= 255; zone++) {
        FieldMap_ZoneSummary_t s;
        FieldMap_GetZoneSummary(zone, &s);

        // Skip zones that have never received events (event_count == 0 OR 0xFFFF from fresh EEPROM)
        if (s.event_count == 0 || s.event_count == 0xFFFF) {
            continue;
        }

        n = snprintf(buf, sizeof(buf),
                     " %3u |   %3u   | %3u | %5u | %3u\r\n",
                     zone, s.latest_disease, s.latest_severity,
                     s.event_count, s.max_severity);
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, n, HAL_MAX_DELAY);
    }
}
