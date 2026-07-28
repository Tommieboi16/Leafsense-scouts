/*
 * LeafSense.h
 *
 *   Created on: 20 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 *
 * Application-layer protocol for the LeafSense Scouts R2R system.
 *
 * Message formats:
 *   EVENT (Field -> Base, 3 bytes):
 *     [0] Message type = 0x01 (DISEASE_EVENT)
 *
 *   Actually the on-wire message is 4 bytes:
 *     [0] MSG_TYPE   — 0xE0 = Event, 0xA0 = Ack
 *     [1] Zone ID    — 1..255 (0 reserved)
 *     [2] Disease class — 0..37 (PlantVillage taxonomy, 0 = healthy)
 *     [3] Severity   — 0..255 (maps to softmax confidence)
 *
 *   Or for ACK:
 *     [0] MSG_TYPE   — 0xA0
 *     [1] Zone ID    — echoed from the event being acknowledged
 *     [2] ACK Status — 0x01=OK stored, 0x02=EEPROM full, 0xFF=auth fail
 *     [3] Reserved   — 0x00
 */

#ifndef INC_LEAFSENSE_H_
#define INC_LEAFSENSE_H_

#include "stdint.h"

/* Message type identifiers (byte 0 of every LeafSense message) */
#define LS_MSG_EVENT        0xE0
#define LS_MSG_ACK          0xA0

/* ACK status values (byte 2 of an ACK message) */
#define LS_ACK_OK           0x01   // received + stored to EEPROM
#define LS_ACK_FULL         0x02   // received but EEPROM page full
#define LS_ACK_AUTH_FAIL    0xFF   // auth or integrity failed

/* On-wire message length */
#define LS_MSG_LEN          4

/* Disease class IDs subset from PlantVillage 38-class taxonomy.
 *kept small for demo. */
#define LS_DISEASE_HEALTHY         0
#define LS_DISEASE_APPLE_SCAB      1
#define LS_DISEASE_APPLE_BLACK_ROT 2
#define LS_DISEASE_TOMATO_BLIGHT   14
#define LS_DISEASE_MAIZE_RUST      22

/* Builder helpers
 * Returns LS_MSG_LEN (4) for chaining with sendFrame() calls. */
int LeafSense_BuildEvent(uint8_t* out, uint8_t zone, uint8_t disease, uint8_t severity);
int LeafSense_BuildAck  (uint8_t* out, uint8_t zone, uint8_t status);

#endif /* INC_LEAFSENSE_H_ */
