/*
 * LeafSense.c
 *
 *   Created on: 20 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 *
 * Implementation of LeafSense protocol message builders.
 */

#include "Leafsense.h"

int LeafSense_BuildEvent(uint8_t* out, uint8_t zone, uint8_t disease, uint8_t severity)
{
    out[0] = LS_MSG_EVENT;   // message type
    out[1] = zone;           // zone ID
    out[2] = disease;        // disease class
    out[3] = severity;       // severity 0..255
    return LS_MSG_LEN;
}

int LeafSense_BuildAck(uint8_t* out, uint8_t zone, uint8_t status)
{
    out[0] = LS_MSG_ACK;     // message type
    out[1] = zone;           // echoed zone ID
    out[2] = status;         // ACK status
    out[3] = 0x00;           // reserved
    return LS_MSG_LEN;
}
