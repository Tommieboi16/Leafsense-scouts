/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "string.h"
#include "EEPROM.h"
#include "Zigbee.h"
#include "LeafSense.h"
#include "Fieldmap.h"
#include "Led.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * ROLE SELECTION
 * Change this ONE line before flashing each Nucleo.
 *   1 = Base Scout  (has EEPROM, receives events, sends ACKs)
 *   2 = Field Scout (no EEPROM, sends events, receives ACKs)
 */
#define ROLE_BASE   1
#define ROLE_FIELD  2

#define ROLE  ROLE_BASE                   // CHANGE THIS PER NUCLEO
/*
 * UID Allow-list
 * Hardcoded UIDs of the two authorised scout Nucleos.
 * Any frame claiming a UID outside this list is rejected.
 */
#define BASE_SCOUT_UID0   0x00200049UL
#define FIELD_SCOUT_UID0  0x0029003EUL
#define BUFF_SIZE           128           //Buffer size for USB comms
#define EEPROM_ADDR         0xA0         //Address for EEPROM
#define EEPROM_PAGE_NUM     256          //Number of pages supported by 24LC64 EEPROM
#define EEPROM_BUFF_SIZE    32           //Number of bytes per page

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char msgBuff[BUFF_SIZE] = {0};                  //A buffer for creating USB messages to the terminal
int len = 0;                                    //An int variable to store the calculated length of messages
char EEPROM_Msg[EEPROM_BUFF_SIZE] = {0};        //A buffer for sending data to the EEPROM
uint8_t ReadBuffer[EEPROM_BUFF_SIZE] = {0};     //A buffer for reading data from the EEPROM
/* nonce for replay protection. Field Scout increments;
 */
uint16_t tx_nonce = 1;
uint32_t my_uid[3];    // This MCU's 96-bit UI

int counter = 0;                                //Counter to produce unique messages over Zigbee
/* Pending event flag — set by UART callback, consumed by main loop */
volatile uint8_t  pending_event_flag     = 0;
volatile uint8_t  pending_event_zone     = 0;
volatile uint8_t  pending_event_disease  = 0;
volatile uint8_t  pending_event_severity = 0;
volatile uint8_t  pending_event_macLow   = 0;
volatile uint8_t  pending_event_mac[4];   // top 4 bytes of sender MAC for printing
volatile uint16_t pending_event_nonce  = 0;
volatile uint8_t  pending_auth_fail_flag = 0;
volatile uint16_t last_nonce_seen      = 0;   // replay protection: highest nonce accepted
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//An example of how to erase all pages in the EEPROM  (erase mean set memory address to 0xFF)
void EraseEntireEEPROM()
{
	for (int i=0; i<EEPROM_PAGE_NUM; i++)
	{
		EEPROM_PageErase(i);
	}
}

void get_xy_from_u32s(const uint32_t uid[3], uint16_t *X, uint16_t *Y)
{
 unsigned char bytes[12];
 memcpy(bytes, uid, 12);                            // STM32 is little-endian; this preserves in-memory order
 // If X uses the first two bytes and Y uses the next two bytes (little-endian):
 *X = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] >> 8));
 *Y = (uint16_t)((uint16_t)bytes[2] | ((uint16_t)bytes[3] >> 8));
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  len = snprintf(msgBuff, BUFF_SIZE, "\r\nProgramme Started");
  HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);

  /* Read this MCU's 96-bit Unique Identifier.
   * Used here for device-level authentication in the security layer. */

  my_uid[0] = HAL_GetUIDw0();
  my_uid[1] = HAL_GetUIDw1();
  my_uid[2] = HAL_GetUIDw2();

  len = snprintf(msgBuff, BUFF_SIZE,
                 "\r\n[UID] This MCU's UID: %08lX %08lX %08lX",
                 my_uid[0], my_uid[1], my_uid[2]);
  HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);

#if (ROLE == ROLE_BASE)
    /* Base Scout: hold WP LOW for the entire session.
     * EEPROM writes are serialised through FieldMap functions in main loop. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);                      // WP LOW permanently
    HAL_Delay(50);                                                // settle time

    uint16_t priorEvents = FieldMap_Init();

    len = snprintf(msgBuff, BUFF_SIZE,
                   "\r\n[BASE] FieldMap ready. Prior events: %u", priorEvents);
    HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);

    FieldMap_DumpToUART();

  #elif (ROLE == ROLE_FIELD)
      len = snprintf(msgBuff, BUFF_SIZE,
                     "\r\n[FIELD] Field Scout ready. Transmit cycle every 2s.");
      HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);
  #endif

  Zigbee_StartReceive();

  Led_Init();
  Led_SetPattern(LED_PATTERN_BOOT);                              // signal boot complete

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

	  /* USER CODE BEGIN 3 */

	  Led_Tick();                                                // always advance LED state machine (non-blocking)

	  #if (ROLE == ROLE_FIELD)
	      /* FIELD SCOUT: send an event every 2 seconds.
	       * Use a simple elapsed-time check instead of HAL_Delay so Led_Tick
	       * keeps running smoothly. */
	      static uint32_t lastTxMs = 0;
	      uint32_t now = HAL_GetTick();

	      if ((now - lastTxMs) >= 2000) {
	          lastTxMs = now;

	          HAL_NVIC_DisableIRQ(USART1_IRQn);

	          uint8_t zone      = 1 + (counter % 10);
	          uint8_t disease   = LS_DISEASE_TOMATO_BLIGHT;
	          uint8_t severity  = 100 + (counter % 100);

	          uint8_t eventMsg[LS_MSG_LEN];
	          int eventLen = LeafSense_BuildEvent(eventMsg, zone, disease, severity);
	          sendSecureFrame(eventMsg, eventLen, tx_nonce, my_uid[0]);
	          tx_nonce++;

	          len = snprintf(msgBuff, BUFF_SIZE,
	                         "\r\n[FIELD] TX secure event: zone=%u disease=%u sev=%u nonce=%u",
	                         zone, disease, severity, (unsigned)(tx_nonce - 1));
	          HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);

	          counter++;
	          HAL_NVIC_EnableIRQ(USART1_IRQn);

	          Led_SetPattern(LED_PATTERN_TX_EVENT);
	      }

#elif (ROLE == ROLE_BASE)
    /* BASE SCOUT main loop */

    /* Handle decryption/integrity failures first */
    if (pending_auth_fail_flag) {
        pending_auth_fail_flag = 0;
        len = snprintf(msgBuff, BUFF_SIZE,
                       "\r\n[BASE] AUTH FAIL: decryption/integrity check rejected");
        HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);
        Led_SetPattern(LED_PATTERN_AUTH_FAIL);
    }

    /* Handle valid events */
    if (pending_event_flag) {
        uint8_t  zone     = pending_event_zone;
        uint8_t  disease  = pending_event_disease;
        uint8_t  severity = pending_event_severity;
        uint8_t  macLow   = pending_event_macLow;
        uint16_t nonce    = pending_event_nonce;
        uint8_t  mac0     = pending_event_mac[0];
        uint8_t  mac1     = pending_event_mac[1];
        uint8_t  mac2     = pending_event_mac[2];
        uint8_t  mac3     = pending_event_mac[3];
        pending_event_flag = 0;

        /* Replay protection: reject if nonce is not strictly higher than last seen.
         * First valid frame (last_nonce_seen == 0) is always accepted. */
        if (last_nonce_seen != 0 && nonce <= last_nonce_seen) {
            len = snprintf(msgBuff, BUFF_SIZE,
                           "\r\n[BASE] REPLAY REJECTED: nonce=%u, last_seen=%u",
                           nonce, last_nonce_seen);
            HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);
            Led_SetPattern(LED_PATTERN_AUTH_FAIL);
            // skip to next loop iteration
        } else {
            last_nonce_seen = nonce;

            len = snprintf(msgBuff, BUFF_SIZE,
                           "\r\n[BASE] RX secure event from %02X%02X%02X%02X: "
                           "zone=%u disease=%u sev=%u nonce=%u",
                           mac0, mac1, mac2, mac3, zone, disease, severity, nonce);
            HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);

            /* Log to EEPROM */
            uint8_t logOk = FieldMap_LogEvent(zone, disease, severity, macLow);

            /* Send ACK back — also encrypted! */
            HAL_NVIC_DisableIRQ(USART1_IRQn);
            uint8_t ackMsg[LS_MSG_LEN];
            uint8_t ackStatus = logOk ? LS_ACK_OK : LS_ACK_FULL;
            int ackLen = LeafSense_BuildAck(ackMsg, zone, ackStatus);
            sendSecureFrame(ackMsg, ackLen, nonce, my_uid[0]);
            HAL_NVIC_EnableIRQ(USART1_IRQn);

            len = snprintf(msgBuff, BUFF_SIZE,
                           "\r\n[BASE] TX secure ack %s for zone=%u",
                           logOk ? "OK" : "FULL", zone);
            HAL_UART_Transmit(&huart2, (uint8_t*)msgBuff, len, 100);

            if (logOk) {
                Led_SetPattern(LED_PATTERN_SUCCESS);
            } else {
                Led_SetPattern(LED_PATTERN_EEPROM_ERROR);
            }
        }
    }
#endif

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        processByte(zigbeeIsrByte);
        HAL_UART_Receive_IT(&huart1, &zigbeeIsrByte, 1);
    }
}

/* Called from the Zigbee state machine when a valid 0x90 frame arrives. */
void onPacketReceived(uint8_t* payload, uint16_t length, uint8_t* srcAddr64)
{
    char out[128];
    int n;

    // Reject anything that's not a LeafSense 4-byte message
    if (length < LS_MSG_LEN) {
        return;
    }



#if (ROLE == ROLE_BASE)
    /* Base Scout: expect 16-byte AES-encrypted block.

     */
    if (length < 16) {
        // Not a secure frame, ignore
        return;
    }

    uint8_t plaintext[4];
    uint16_t nonce = 0;
    uint32_t sender_uid = 0;
    if (!verifySecureFrame(payload, plaintext, &nonce, &sender_uid)) {
        pending_auth_fail_flag = 1;
        return;
    }

    /* UID allow-list check: Base Scout only accepts frames from the known Field Scout */
    if (sender_uid != FIELD_SCOUT_UID0) {
        pending_auth_fail_flag = 1;
        return;
    }

    uint8_t msgType = plaintext[0];
    if (msgType == LS_MSG_EVENT) {
        pending_event_zone     = plaintext[1];
        pending_event_disease  = plaintext[2];
        pending_event_severity = plaintext[3];
        pending_event_macLow   = srcAddr64[7];
        pending_event_mac[0]   = srcAddr64[4];
        pending_event_mac[1]   = srcAddr64[5];
        pending_event_mac[2]   = srcAddr64[6];
        pending_event_mac[3]   = srcAddr64[7];
        pending_event_nonce    = nonce;         // new
        pending_event_flag     = 1;
    }
#endif

#if (ROLE == ROLE_FIELD)
    /* Field Scout: expect 16-byte AES-encrypted ACK */
    if (length < 16) return;

    uint8_t plaintext[4];
    uint16_t ackNonce = 0;
    uint32_t sender_uid = 0;
    if (!verifySecureFrame(payload, plaintext, &ackNonce, &sender_uid)) {
        return;
    }

    /* UID allow-list check: Field Scout only accepts ACKs from the known Base Scout */
    if (sender_uid != BASE_SCOUT_UID0) {

        return;
    }

    uint8_t msgType = plaintext[0];
    if (msgType == LS_MSG_ACK) {
        uint8_t zone   = plaintext[1];
        uint8_t status = plaintext[2];
        const char* statusStr = (status == LS_ACK_OK)   ? "OK"
                              : (status == LS_ACK_FULL) ? "FULL"
                              : (status == LS_ACK_AUTH_FAIL) ? "AUTH_FAIL"
                              : "UNKNOWN";

        n = snprintf(out, sizeof(out),
                     "\r\n[FIELD] RX secure ack: zone=%u status=%s nonce=%u",
                     zone, statusStr, ackNonce);
        HAL_UART_Transmit(&huart2, (uint8_t*)out, n, 100);
    }
#endif
}

/* USER CODE END 4 */


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
