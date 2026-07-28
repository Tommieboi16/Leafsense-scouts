/*
 * EEPROM.h
 *
 *   Created on: 17 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 * Developed for: Module 6ENT1180 EEPROM 24LC64 and Nucleo-F411RE
 * Hardware Reference: https://docs.rs-online.com/a887/0900766b8137dd96.pdf
 */

#ifndef INC_EEPROM_H_
#define INC_EEPROM_H_


#include "stdint.h"
#include "stm32f4xx_hal.h"

/*
 * Method declarations that can be called from main.c
 */
void EEPROM_Write (uint16_t page, uint16_t offset, uint8_t *data, uint16_t size);
void EEPROM_Read  (uint16_t page, uint16_t offset, uint8_t *data, uint16_t size);

void EEPROM_Write_NUM (uint16_t page, uint16_t offset, float  fdata);
float EEPROM_Read_NUM (uint16_t page, uint16_t offset);

void EEPROM_PageErase (uint16_t page);


#endif /* INC_EEPROM_H_ */
