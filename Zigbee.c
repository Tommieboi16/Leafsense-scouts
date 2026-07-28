/*
 * Zigbee.c
 *
 *   Created on: 18 April 2026
 *       Author: Tomiwa (Student ID: 22044811) – University of Hertfordshire
 * Developed for: Module 6ENT1180 Zigbee (XBee S2C) and Nucleo-F411RE
 * Hardware Reference: https://docs.digi.com/resources/documentation/digidocs/pdfs/90002002.pdf
 */

//Includes
#include "Zigbee.h"
#include "aes.h"


//Defines and Externs
extern UART_HandleTypeDef   huart1;
#define XBEE_USART          &huart1

#define RX_FRAME_MAX        256    // maximum assembled frame size (body only)

/* Application-layer AES-128 key.
 */
static const uint8_t APP_AES_KEY[16] = {
    0x4C, 0x65, 0x61, 0x66,    // 'L','e','a','f'
    0x53, 0x65, 0x6E, 0x73,    // 'S','e','n','s'
    0x65, 0x41, 0x67, 0x72,    // 'e','A','g','r'
    0x69, 0x32, 0x30, 0x32     // 'i','2','0','2'
};

static struct AES_ctx g_aes_ctx;
static uint8_t        g_aes_initialised = 0;

/* initialise the AES context on first use. */
static void ensure_aes_ctx(void)
{
    if (!g_aes_initialised) {
        AES_init_ctx(&g_aes_ctx, APP_AES_KEY);
        g_aes_initialised = 1;
    }
}
//Receive state machine state
typedef enum {
    RX_IDLE,                            // waiting for 0x7E start delimiter
    RX_GOT_DELIMITER,                   // got 0x7E, waiting for length MSB
    RX_GOT_LEN_MSB,                     // got length MSB, waiting for length LSB
    RX_READING_BODY,                    // reading the frame body (length bytes)
    RX_READING_CKSM                     // reading the checksum byte
} RxState_t;

static RxState_t rxState        = RX_IDLE;
static uint8_t   rxBuffer[RX_FRAME_MAX]; // body bytes stored from index 3 onwards to match datasheet indexing
static uint16_t  rxFrameLength  = 0;     // length field value
static uint16_t  rxBytesRead    = 0;     // how many body bytes we've stored
uint8_t zigbeeIsrByte = 0;               // one-byte buffer for HAL interrupt (exposed to main.c callback)

//Variables




//Methods

/* The checksum is a security feature to ensure that data is as intended, unchanged, when it reaches the recipient.Datasheet page 197
 * @param message - Complete data frame buffer, with body starting at index 3
 * @param length  - Final total length of the data frame buffer
 */
uint8_t calculateChecksum(uint8_t* message, int length)
{
    int temp = 0;                       // Storage variable used in the calculation
    for(int i=3; i<length-1; i++)       // For loop to process each byte from index 3 to the last payload byte
    {
        temp += message[i];             // Adding the byte numbers together
    }
    return (0xFF - temp) & 0xFF;        // 0xFF - sum of all bytes, then return the last byte of the number only
}


/* Builds a 0x10 Transmit Request API frame with a 16-bit broadcast destination
 * (0xFFFE = unknown), ships it to the XBee over huart1.
 * @param message - pointer to the RF data payload
 * @param length  - length of the RF data payload in bytes
 */
void sendFrame(uint8_t* message, int length)
{
    uint8_t txFrame[256];                                       // Adjust the array size as per your requirement

    txFrame[0] = 0x7E;                                          // Start delimiter per page 171 of hardware reference
    uint16_t frameLength = length + 14;                         // Body length = 14 header bytes + payload
    txFrame[1] = (frameLength >> 8) & 0xFF;                     // MSB of frame length
    txFrame[2] = frameLength & 0xFF;                            // LSB of frame length

    txFrame[3] = 0x10;                                          // Frame type (Transmit Request)
    txFrame[4] = 0x01;                                          // Frame ID (nonzero = we want a TX Status back)

    txFrame[5]  = 0x00;                                         // 64-bit Destination Address
    txFrame[6]  = 0x00;
    txFrame[7]  = 0x00;
    txFrame[8]  = 0x00;
    txFrame[9]  = 0x00;
    txFrame[10] = 0x00;
    txFrame[11] = 0xFF;
    txFrame[12] = 0xFF;                                         // 0x000000000000FFFF = broadcast MAC

    txFrame[13] = 0xFF;                                         // 16-bit Destination Address
    txFrame[14] = 0xFE;                                         // 0xFFFE = "don't know the short address"

    txFrame[15] = 0x00;                                         // Broadcast Radius (0 = max)
    txFrame[16] = 0x00;                                         // Options

    memcpy(&txFrame[17], message, length);                      // Copy the payload

    int finalLen = 17 + length + 1;                             // header + payload + checksum
    uint8_t chksum = calculateChecksum(txFrame, finalLen);
    txFrame[finalLen-1] = chksum;

    HAL_UART_Transmit(XBEE_USART, txFrame, finalLen, 100);
}


/* Arms the UART1 interrupt to receive one byte at a time.
 * The HAL_UART_RxCpltCallback in main.c feeds that byte into processByte()
 * and re-arms the next receive.
 * Must be called ONCE after MX_USART1_UART_Init().
 */
void Zigbee_StartReceive(void)
{
    rxState = RX_IDLE;
    HAL_UART_Receive_IT(XBEE_USART, &zigbeeIsrByte, 1);
}


/* Single-byte ingest for the API frame state machine.
 * Call from the UART RX interrupt (HAL_UART_RxCpltCallback).
 * When a complete, checksum-valid 0x90 Receive Packet frame is assembled,
 * onPacketReceived() is called with a pointer to the RF data payload,
 * the payload length, and the sender's 64-bit MAC address.
 * Frames with other types (e.g. 0x8B TX Status) are silently discarded —
 * they do NOT reach the application callback.
 */
void processByte(uint8_t byte)
{
    switch (rxState)
    {
        case RX_IDLE:
            // Only a 0x7E delimiter starts a new frame. Anything else is noise.
            if (byte == 0x7E) {
                rxState = RX_GOT_DELIMITER;
            }
            break;

        case RX_GOT_DELIMITER:
            // High byte of length
            rxFrameLength = ((uint16_t)byte) << 8;
            rxState = RX_GOT_LEN_MSB;
            break;

        case RX_GOT_LEN_MSB:
            // Low byte of length
            rxFrameLength |= byte;

            // Sanity check — if the claimed length is ridiculous, abandon this frame
            if (rxFrameLength == 0 || rxFrameLength > (RX_FRAME_MAX - 4)) {
                rxState = RX_IDLE;
                break;
            }

            // Start filling the body at buffer index 3 so byte positions match the checksum spec.
            rxBytesRead = 0;
            rxState = RX_READING_BODY;
            break;

        case RX_READING_BODY:
            rxBuffer[3 + rxBytesRead] = byte;
            rxBytesRead++;
            if (rxBytesRead >= rxFrameLength) {
                rxState = RX_READING_CKSM;
            }
            break;

        case RX_READING_CKSM:
        {
            // Verify checksum: 0xFF - (sum of body bytes) should equal the received checksum
            uint32_t sum = 0;
            for (uint16_t i = 0; i < rxFrameLength; i++) {
                sum += rxBuffer[3 + i];
            }
            uint8_t expected = (0xFF - (sum & 0xFF)) & 0xFF;

            if (expected == byte) {
                // Valid frame — inspect the frame type.
                uint8_t frameType = rxBuffer[3];
                if (frameType == 0x90) {
                    // 0x90 Receive Packet layout (datasheet p.206):
                    //   [3]      Frame type = 0x90
                    //   [4..11]  64-bit source address (MSB first)
                    //   [12..13] 16-bit source address
                    //   [14]     Receive options
                    //   [15..]   RF data payload
                    uint8_t* payload    = &rxBuffer[15];
                    uint16_t payloadLen = rxFrameLength - 12;
                    uint8_t* srcAddr64  = &rxBuffer[4];
                    onPacketReceived(payload, payloadLen, srcAddr64);
                }
                // Other frame types (e.g. 0x8B TX Status) are silently dropped.
            }
            // else checksum failed — silently discard and resync.

            rxState = RX_IDLE;
            break;
        }
    }
}

/* =====================================================================
 * Application-layer secure send/receive
 * =====================================================================
 *
 * Plaintext block layout (16 bytes = one AES-128 block):
 *   [0..3]  Original payload bytes (padded with zeros if shorter)
 *   [4]     Nonce low byte
 *   [5]     Nonce high byte
 *   [6..15] Structured zero padding (integrity check: must decrypt to 0x00)
 *
 * Using ECB is acceptable here because the nonce guarantees each plaintext
 * block is unique, neutralising ECB's correlation weakness.
 */

void sendSecureFrame(uint8_t* plaintext, int length, uint16_t nonce, uint32_t sender_uid0)
{
    uint8_t block[16];
    memset(block, 0x00, 16);

    int copyLen = (length > 4) ? 4 : length;
    memcpy(block, plaintext, copyLen);

    block[4] = (uint8_t)(nonce & 0xFF);
    block[5] = (uint8_t)((nonce >> 8) & 0xFF);

    // UID[0] embedded little-endian in bytes 6..9
    block[6] = (uint8_t)(sender_uid0 & 0xFF);
    block[7] = (uint8_t)((sender_uid0 >> 8) & 0xFF);
    block[8] = (uint8_t)((sender_uid0 >> 16) & 0xFF);
    block[9] = (uint8_t)((sender_uid0 >> 24) & 0xFF);
    // bytes 10..15 remain zero — integrity padding

    ensure_aes_ctx();
    AES_ECB_encrypt(&g_aes_ctx, block);

    sendFrame(block, 16);
}

uint8_t verifySecureFrame(uint8_t* ciphertext, uint8_t* plaintextOut, uint16_t* nonceOut, uint32_t* senderUidOut)
{
    if (ciphertext == NULL || plaintextOut == NULL) return 0;

    uint8_t block[16];
    memcpy(block, ciphertext, 16);

    ensure_aes_ctx();
    AES_ECB_decrypt(&g_aes_ctx, block);

    // Integrity check: bytes 10..15 must decrypt to zero
    for (int i = 10; i < 16; i++) {
        if (block[i] != 0x00) {
            return 0;
        }
    }

    if (nonceOut) {
        *nonceOut = ((uint16_t)block[5] << 8) | block[4];
    }

    if (senderUidOut) {
        *senderUidOut = ((uint32_t)block[9] << 24) |
                        ((uint32_t)block[8] << 16) |
                        ((uint32_t)block[7] << 8)  |
                         (uint32_t)block[6];
    }

    memcpy(plaintextOut, block, 4);

    return 1;
}
