/*
 * Zigbee.h
 *
 *   Created on: 17 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 * Developed for: Module 6ENT1180 Zigbee (XBee S2C) and Nucleo-F411RE
 * Hardware Reference: https://docs.digi.com/resources/documentation/digidocs/pdfs/90002002.pdf
 */

#ifndef INC_ZIGBEE_H_
#define INC_ZIGBEE_H_

#include "stdint.h"
#include "string.h"
#include "stm32f4xx_hal.h"

/* Public API */
uint8_t calculateChecksum(uint8_t* message, int length);
void    sendFrame(uint8_t* message, int length);

void    Zigbee_StartReceive(void);
void    processByte(uint8_t byte);

/* Application callback */
void    onPacketReceived(uint8_t* payload, uint16_t length, uint8_t* srcAddr64);

/* Exposed one-byte buffer for the UART ISR, owned by Zigbee.c */
extern uint8_t zigbeeIsrByte;

#endif /* INC_ZIGBEE_H_ */

/* Application-layer security
 */
void    sendSecureFrame(uint8_t* plaintext, int length, uint16_t nonce, uint32_t sender_uid0);
uint8_t verifySecureFrame(uint8_t* ciphertext, uint8_t* plaintextOut, uint16_t* nonceOut, uint32_t* senderUidOut);
