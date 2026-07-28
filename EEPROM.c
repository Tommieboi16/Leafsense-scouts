/*
 * EEPROM.c
 *
 *   Created on: 17 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 * Developed for: Module 6ENT1180 EEPROM 24LC64 and Nucleo-F411RE
 * Hardware Reference: https://docs.rs-online.com/a887/0900766b8137dd96.pdf
 */

//Includes
#include "EEPROM.h"
#include "math.h"
#include "string.h"


//Defines and Externs
extern I2C_HandleTypeDef  hi2c1;             //Make sure this matches the I2C object created by STM32CubeMX when enabling I2C on the Nucleo
#define EEPROM_I2C        &hi2c1             //Pointer reference for succinct code changes in the future. Changing the I2C reference here changes in all references afterward
#define EEPROM_ADDR       0xA0               //This is the EEPROM Address "A" is default and "0" is configured by pullup/pulldown on pins A0-A2 (Datasheet Page 5)
#define PAGE_SIZE         32                 //The page size is defined in the datasheet for the EEPROM. 32 Bytes for 24LC64 (Datasheet Page 1 - Description). Can be changed for other chips.
#define PAGE_NUM          256                //This is the number of pages supported by the 24LC64 - 64kbit = 8192 bytes - 8192/32=256. Can be changed for other chips.

//Variables
uint8_t bytes_temp[4];

/* A union object that utilises a 4-byte memory location to easily convert a float to an array of bytes that is always 4 bytes long
 * float was the example used in the lecture for a Union
 */
union {
    float FloatValue;
    uint8_t BytesValue[4];
} FloatUnion;

/* Calculates the number of bytes to read/write in the next process
 * If your data exceeds the page size you need to manage reading/writing across multiple pages
 * If you do not, the excess data will be actioned (read/written) from the start index of the page currently being actioned.
 * E.g., you write 35 bytes in Page 0 from Byte 0, you will fill up Page 0 with the first 32 bytes,
 * then the remaining 3 bytes will be written into Page 0, Bytes 0-2, overwriting what you already put in 0-2.
 * This method helps manage this issue, as long as size and offset change to suit.
 */
uint16_t BytesToAction (uint16_t size, uint16_t offset)
{
    if ((size+offset)<PAGE_SIZE) return size;
    else return PAGE_SIZE-offset;
}


/* Writes data to the EEPROM
 * @page is the start page. From 0 to PAGE_NUM-1
 * @offset is the start byte offset in the page. From 0 to PAGE_SIZE-1, e.g., you want just to write from the third byte in the page, this would be 2.
 * @data is the pointer to the bytes you are writing to the chip
 * @size is the size of the data. Minimum 1 byte.
 */
void EEPROM_Write (uint16_t page, uint16_t offset, uint8_t *data, uint16_t size)
{
    int AddressPosition = log(PAGE_SIZE)/log(2);                   //The number of bits in a byte-level address per page, e.g., in 24LC64 log2(32) = 5-bit address per page
    uint16_t StartPage  = page;                                    //Start page index defined in parameters
    uint16_t EndPage    = page + ((size+offset)/PAGE_SIZE);        //End page index calculated based on size of data required
    uint16_t NumOfPages = (EndPage-StartPage) + 1;                 //Number of pages to be written, as indicated above
    uint16_t pos = 0;                                              //A counter for writing bytes (position)

    // Loop through the number of required pages and write the data
    for (int i=0; i<NumOfPages; i++)
    {
        uint16_t MemAddress = StartPage<<AddressPosition | offset; //Calculate the Byte-level address and combine with the Page number to create the memory address
        uint16_t NumBytes   = BytesToAction(size, offset);         //Calculate the bytes being written now in this looped page write

        HAL_I2C_Mem_Write(EEPROM_I2C, EEPROM_ADDR, MemAddress, 2, &data[pos], NumBytes, 1000); //Raw I2C write function within HAL Interface. Address occupies 2 bytes.

        StartPage += 1;           //Increment page count by 1 - if the number of pages required is more than 1, this will enable the next sequential page to be used for the next bytes
        offset = 0;               //Return offset to zero, as we will start the next page utilisation at the zero byte index
        size = size-NumBytes;     //Subtract written bytes so we can track remaining data
        pos += NumBytes;          //Increment the position counter to the next chunk of remaining bytes to write
        HAL_Delay (5);            //Implement a delay of 5ms to enable the EEPROM time to action the write to memory. Defined in datasheet Page 4, Table 1-2, Parameter number 17.
    }
}

/* A conversion method which demonstrates how to use a Union to easily convert different variable types to bytes (uint8_t)
 * The Union utilises the same memory location for two or more variable types. Float can be used to support integers also.
 */
void float2Bytes(uint8_t * ftob_bytes_out, float float_variable)
{
    FloatUnion.FloatValue = float_variable;         //Use Union to convert float to bytes

    for (uint8_t i = 0; i < 4; i++) {
        ftob_bytes_out[i] = FloatUnion.BytesValue[i];  //Byte for byte copy to output array pointer - Can be replaced with memcpy if preferred. This approach is to illustrate what is being done.
    }
    //No return type (void) as the pointer is an output parameter in this example.
}

/* A conversion method which demonstrates how to use a Union to easily convert bytes (uint8_t) to different variable types
 * The Union utilises the same memory location for two or more variable types. Float can be used to support integers also.
 */
float Bytes2float(uint8_t * btof_bytes_in)
{
    for (uint8_t i = 0; i < 4; i++) {
        FloatUnion.BytesValue[i] = btof_bytes_in[i];   //Byte for byte copy from input array pointer - Can be replaced with memcpy if preferred. This approach is to illustrate what is being done.
    }

    float float_variable = FloatUnion.FloatValue;      //Get the float version of the data
    return float_variable;                             //Return the result
}

/* An example of writing a float to the EEPROM using a concise method to call the Union to convert and then write
 * @page is the start page index from 0 to PAGE_NUM-1
 * @offset is the start byte index in the page from 0 to PAGE_SIZE-1
 * @data is the float or integer value to be written
 */
void EEPROM_Write_NUM (uint16_t page, uint16_t offset, float data)
{
    float2Bytes(bytes_temp, data);          //Call the conversion method that uses a Union
    EEPROM_Write(page, offset, bytes_temp, 4);  //Write the data that has been converted
}

/* Reads the float or int values previously written
 * @page is the start page index from 0 to PAGE_NUM-1
 * @offset is the start byte index in the page from 0 to PAGE_SIZE-1
 * @returns the float or integer value as a float
 */
float EEPROM_Read_NUM (uint16_t page, uint16_t offset)
{
    uint8_t ReadBuffer[4];                      //Working array declaration
    EEPROM_Read(page, offset, ReadBuffer, 4);   //Call the read function noting the requires memory location (page index and byte start index)
    return (Bytes2float(ReadBuffer));           //Return the result as a float converted using a Union
}

/* READ the data from the EEPROM
 * @page is the start page index from 0 to PAGE_NUM-1
 * @offset is the start byte index from 0 to PAGE_SIZE-1
 * @data is the pointer to the data in Bytes
 * @size is the size of the data to be read in Bytes
 */
void EEPROM_Read (uint16_t page, uint16_t offset, uint8_t *data, uint16_t size)
{
    int AddressPosition = log(PAGE_SIZE)/log(2);                   //The number of bits in a byte-level address per page, e.g., in 24LC64 log2(32) = 5-bit address per page
    uint16_t StartPage  = page;                                    //Start page index defined in parameters
    uint16_t EndPage    = page + ((size+offset)/PAGE_SIZE);        //End page index calculated based on size of data required
    uint16_t NumOfPages = (EndPage-StartPage) + 1;                 //Number of pages to be read, as indicated above
    uint16_t pos = 0;                                              //A counter for reading bytes (position)

    // Loop through the number of required pages and read the data
    for (int i=0; i<NumOfPages; i++)
    {
        uint16_t MemAddress = StartPage<<AddressPosition | offset; //Calculate the Byte-level address and combine with the Page number to create the memory address
        uint16_t NumBytes   = BytesToAction(size, offset);         //Calculate the bytes being read now in this looped page read

        HAL_I2C_Mem_Read(EEPROM_I2C, EEPROM_ADDR, MemAddress, 2, &data[pos], NumBytes, 1000); // Raw I2C read function within HAL Interface. Address occupies 2 bytes.

        StartPage += 1;           //Increment page count by 1 - if the number of pages required is more than 1, this will enable the next sequential page to be used for the next bytes
        offset = 0;               //Return offset to zero, as we will start the next page utilisation at the zero byte index
        size = size-NumBytes;     //Subtract read bytes so we can track remaining data
        pos += NumBytes;          //Increment the position counter to the next chunk of remaining bytes to read
                                  //No delay necessary, as the 5ms used in write applies to write only
    }
}

/* Example of how to erase a whole page in the EEPROM. You can use the earlier write function to "erase" specific bytes, but writing 0xFF as the data.
 * @page is the number of pages intended to erase
 * Call this in a loop if there are multiple pages to erase
 */
void EEPROM_PageErase (uint16_t page)
{
    int AddressPosition = log(PAGE_SIZE)/log(2);      //The number of bits in a byte-level address per page, e.g., in 24LC64 log2(32) = 5-bit address per page
    uint16_t MemAddress = page<<AddressPosition;      //Directly combine to get Page address, since this method does not handle multiple pages or specific Byte deletions
    uint8_t data[PAGE_SIZE];                          //Working buffer array for reset values
    memset(data,0xff,PAGE_SIZE);                      //Set buffer to all 0xFF (unused/deleted)

    HAL_I2C_Mem_Write(EEPROM_I2C, EEPROM_ADDR, MemAddress, 2, data, PAGE_SIZE, 1000); //Write the erasure data to the EEPROM. Address occupies 2 bytes.

    HAL_Delay (5);    //Implement a delay of 5ms to enable the EEPROM time to action the write to memory. Defined in datasheet Page 4, Table 1-2, Parameter number 17.
}
