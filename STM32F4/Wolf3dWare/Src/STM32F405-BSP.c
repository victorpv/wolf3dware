#ifndef USE_STM32F429I_DISCO

#include "STM32F405-BSP.h"

//#define USE_SPI_I2C

GPIO_TypeDef* GPIO_PORT[LEDn] = {LED4_GPIO_PORT, LED3_GPIO_PORT};
const uint16_t GPIO_PIN[LEDn] = {LED4_PIN, LED3_PIN};

GPIO_TypeDef* BUTTON_PORT[BUTTONn] = {KEY_BUTTON_GPIO_PORT};
const uint16_t BUTTON_PIN[BUTTONn] = {KEY_BUTTON_PIN};
const uint8_t BUTTON_IRQn[BUTTONn] = {KEY_BUTTON_EXTI_IRQn};

#ifdef USE_SPI
/*############################### SPI1 #######################################*/
uint32_t SpixTimeout = SPIx_TIMEOUT_MAX;    /*<! Value of Timeout when SPI communication fails */
static SPI_HandleTypeDef    SpiHandle;
static void     SPIx_Init(void);
static void     SPIx_MspInit(void);
static uint8_t  SPIx_WriteRead(uint8_t Byte);
static  void    SPIx_Error(void);
#define SPIx                              SPI1
#define SPIx_CLK_ENABLE()                 __SPI1_CLK_ENABLE()
#define SPIx_GPIO_PORT                    GPIOA                      /* GPIOA */
#define SPIx_AF                           GPIO_AF5_SPI1
#define SPIx_GPIO_CLK_ENABLE()            __GPIOA_CLK_ENABLE()
#define SPIx_GPIO_CLK_DISABLE()           __GPIOA_CLK_DISABLE()
#define SPIx_SCK_PIN                      GPIO_PIN_5                 /* PA.05 */
#define SPIx_MISO_PIN                     GPIO_PIN_6                 /* PA.06 */
#define SPIx_MOSI_PIN                     GPIO_PIN_7                 /* PA.07 */

/* Maximum Timeout values for flags waiting loops. These timeouts are not based
   on accurate values, they just guarantee that the application will not remain
   stuck if the SPI communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */
#define SPIx_TIMEOUT_MAX                            0x1000 /*<! The value of the maximal timeout for BUS waiting loops */
#endif

/*############################# I2C1 #########################################*/

uint32_t I2cxTimeout =  0x3000;    /*<! Value of Timeout when I2C communication fails */
static I2C_HandleTypeDef    I2cHandle;
static void I2Cx_Error(uint8_t Addr);

/* I2C clock speed configuration (in Hz) */
#ifndef BSP_I2C_SPEED
#define BSP_I2C_SPEED 100000
#endif /* BSP_I2C_SPEED */

/* I2C peripheral configuration defines */
#define I2Cx                            I2C1
#define I2Cx_CLK_ENABLE()               __I2C1_CLK_ENABLE()
#define I2Cx_SCL_SDA_GPIO_CLK_ENABLE()  __GPIOB_CLK_ENABLE()
#define I2Cx_SCL_SDA_AF                 GPIO_AF4_I2C1
#define I2Cx_SCL_SDA_GPIO_PORT          GPIOB
#define I2Cx_SCL_PIN                    GPIO_PIN_8
#define I2Cx_SDA_PIN                    GPIO_PIN_9

#define I2Cx_FORCE_RESET()              __I2C1_FORCE_RESET()
#define I2Cx_RELEASE_RESET()            __I2C1_RELEASE_RESET()

/* I2C interrupt requests */
#define I2Cx_EV_IRQn                    I2C1_EV_IRQn
#define I2Cx_ER_IRQn                    I2C1_ER_IRQn

/* Maximum Timeout values for flags waiting loops. These timeouts are not based
   on accurate values, they just guarantee that the application will not remain
   stuck if the SPI communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */
#define I2Cx_TIMEOUT_MAX    0x1000 /*<! The value of the maximal timeout for BUS waiting loops */

void BSP_LED_Init(Led_TypeDef Led)
{
  GPIO_InitTypeDef  GPIO_InitStruct;

  /* Enable the GPIO_LED Clock */
  LEDx_GPIO_CLK_ENABLE(Led);

  /* Configure the GPIO_LED pin */
  GPIO_InitStruct.Pin = GPIO_PIN[Led];
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;

  HAL_GPIO_Init(GPIO_PORT[Led], &GPIO_InitStruct);

  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_RESET);
}

/**
  * @brief  Turns selected LED On.
  * @param  Led: Specifies the Led to be set on.
  *   This parameter can be one of following parameters:
  *     @arg LED4
  *     @arg LED3
  *     @arg LED5
  *     @arg LED6
  * @retval None
  */
void BSP_LED_On(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_SET);
}

/**
  * @brief  Turns selected LED Off.
  * @param  Led: Specifies the Led to be set off.
  *   This parameter can be one of following parameters:
  *     @arg LED4
  *     @arg LED3
  *     @arg LED5
  *     @arg LED6
  * @retval None
  */
void BSP_LED_Off(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_RESET);
}

/**
  * @brief  Toggles the selected LED.
  * @param  Led: Specifies the Led to be toggled.
  *   This parameter can be one of following parameters:
  *     @arg LED4
  *     @arg LED3
  *     @arg LED5
  *     @arg LED6
  * @retval None
  */
void BSP_LED_Toggle(Led_TypeDef Led)
{
  HAL_GPIO_TogglePin(GPIO_PORT[Led], GPIO_PIN[Led]);
}

/**
  * @}
  */

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_BUTTON_Functions
  * @{
  */

/**
  * @brief  Configures Button GPIO and EXTI Line.
  * @param  Button: Specifies the Button to be configured.
  *   This parameter should be: BUTTON_KEY
  * @param  Mode: Specifies Button mode.
  *   This parameter can be one of following parameters:
  *     @arg BUTTON_MODE_GPIO: Button will be used as simple IO
  *     @arg BUTTON_MODE_EXTI: Button will be connected to EXTI line with interrupt
  *                            generation capability
  * @retval None
  */
void BSP_PB_Init(Button_TypeDef Button, ButtonMode_TypeDef Mode)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  /* Enable the BUTTON Clock */
  BUTTONx_GPIO_CLK_ENABLE(Button);

  if (Mode == BUTTON_MODE_GPIO)
  {
    /* Configure Button pin as input */
    GPIO_InitStruct.Pin = BUTTON_PIN[Button];
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;

    HAL_GPIO_Init(BUTTON_PORT[Button], &GPIO_InitStruct);
  }

  if (Mode == BUTTON_MODE_EXTI)
  {
    /* Configure Button pin as input with External interrupt */
    GPIO_InitStruct.Pin = BUTTON_PIN[Button];
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    HAL_GPIO_Init(BUTTON_PORT[Button], &GPIO_InitStruct);

    /* Enable and set Button EXTI Interrupt to the lowest priority */
    HAL_NVIC_SetPriority((IRQn_Type)(BUTTON_IRQn[Button]), 0x0F, 0);
    HAL_NVIC_EnableIRQ((IRQn_Type)(BUTTON_IRQn[Button]));
  }
}

/**
  * @brief  Returns the selected Button state.
  * @param  Button: Specifies the Button to be checked.
  *   This parameter should be: BUTTON_KEY
  * @retval The Button GPIO pin value.
  */
uint32_t BSP_PB_GetState(Button_TypeDef Button)
{
  return HAL_GPIO_ReadPin(BUTTON_PORT[Button], BUTTON_PIN[Button]);
}

////////////////////////////////////////////////////////////////////
// Encoder API

static TIM_HandleTypeDef Encoder_Handle;
static TIM_Encoder_InitTypeDef sEncoderConfig;

// Quadrature encoder using TIM8 ch1 ch2 PC6 PC7
int BSP_Init_Encoder()
{
	/* -1- Initialize TIM8 to handle the encoder sensor */
	/* Initialize TIM8 peripheral as follow:
	     + Period = 65535
	     + Prescaler = 0
	     + ClockDivision = 0
	     + Counter direction = Up
	*/
	Encoder_Handle.Instance = TIM8;
	Encoder_Handle.Init.Period             = 65535;
	Encoder_Handle.Init.Prescaler          = 0;
	Encoder_Handle.Init.ClockDivision      = 0;
	Encoder_Handle.Init.CounterMode        = TIM_COUNTERMODE_UP;
	Encoder_Handle.Init.RepetitionCounter  = 0;

	sEncoderConfig.EncoderMode        = TIM_ENCODERMODE_TI12;

	sEncoderConfig.IC1Polarity        = TIM_ICPOLARITY_RISING;
	sEncoderConfig.IC1Selection       = TIM_ICSELECTION_DIRECTTI;
	sEncoderConfig.IC1Prescaler       = TIM_ICPSC_DIV1;
	sEncoderConfig.IC1Filter          = 3;

	sEncoderConfig.IC2Polarity        = TIM_ICPOLARITY_RISING;
	sEncoderConfig.IC2Selection       = TIM_ICSELECTION_DIRECTTI;
	sEncoderConfig.IC2Prescaler       = TIM_ICPSC_DIV1;
	sEncoderConfig.IC2Filter          = 3;

	if(HAL_TIM_Encoder_Init(&Encoder_Handle, &sEncoderConfig) != HAL_OK)
	{
		return 0;
	}

	/* Start the encoder interface */
	HAL_TIM_Encoder_Start(&Encoder_Handle, TIM_CHANNEL_ALL);
  __HAL_TIM_SET_COUNTER(&Encoder_Handle, 32768);
	return 1;
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
	if(htim->Instance != TIM8) return; // not us

	GPIO_InitTypeDef   GPIO_InitStruct;

	/*##-1- Enable peripherals and GPIO Clocks #################################*/
	/* TIM8 Peripheral clock enable */
	__HAL_RCC_TIM8_CLK_ENABLE();

	/* Enable GPIO Channels Clock */
	__HAL_RCC_GPIOC_CLK_ENABLE();

	/*##-2- Configure I/Os #####################################################*/
	/* Common configuration for all channels */
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF3_TIM8;

	/* Channel 1 & 2 configuration */
	GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

uint16_t BSP_Read_Encoder()
{
	//uint32_t uwDirection = __HAL_TIM_IS_TIM_COUNTING_DOWN(&Encoder_Handle);

	return (__HAL_TIM_GET_COUNTER(&Encoder_Handle) & 0xFFFF);
}

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_BUS_Functions
  * @{
  */


/******************************* I2C Routines**********************************/
/**
  * @brief  Configures I2C interface.
  * @param  None
  * @retval None
  */
void I2Cx_Init(void)
{
  if(HAL_I2C_GetState(&I2cHandle) == HAL_I2C_STATE_RESET)
  {
    /* 2Cx peripheral configuration */
    I2cHandle.Init.ClockSpeed = BSP_I2C_SPEED;
    I2cHandle.Init.DutyCycle = I2C_DUTYCYCLE_2;
    I2cHandle.Init.OwnAddress1 = 0x33;
    I2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    I2cHandle.Instance = I2Cx;

    /* Init the I2C */
    HAL_I2C_Init(&I2cHandle);
  }
}

/**
  * @brief  Write a value in a register of the device through BUS.
  * @param  Addr: Device address on BUS Bus.
  * @param  Data: The dtata buffer top be written
  * @param  Len: The number of bytes to send
  * @retval HAL status
  */
bool I2Cx_WriteData(uint8_t Addr, uint8_t *Data, uint16_t Len)
{
  HAL_StatusTypeDef status = HAL_OK;

  status = HAL_I2C_Master_Transmit(&I2cHandle, Addr, Data, Len, I2cxTimeout);

  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* Execute user timeout callback */
    I2Cx_Error(Addr);
    return false;
  }

  return true;
}

/**
  * @brief  Read a register of the device through BUS
  * @param  Addr: Device address on BUS
  * @param  Reg: The target register address to read
  * @retval HAL status
  */
bool I2Cx_ReadData(uint8_t Addr, uint8_t *Data, uint16_t Len)
{
  HAL_StatusTypeDef status = HAL_OK;

  status = HAL_I2C_Master_Receive(&I2cHandle, Addr, Data, Len, I2cxTimeout);

  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* Execute user timeout callback */
    I2Cx_Error(Addr);
    return false;
  }

  return true;
}

/**
  * @brief  Manages error callback by re-initializing I2C.
  * @param  Addr: I2C Address
  * @retval None
  */
static void I2Cx_Error(uint8_t Addr)
{
  /* De-initialize the I2C communication bus */
  HAL_I2C_DeInit(&I2cHandle);

  /* Re-Initialize the I2C communication bus */
  I2Cx_Init();
}

/**
  * @brief I2C MSP Initialization
  * @param None
  * @retval None
  */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  GPIO_InitTypeDef  GPIO_InitStruct;

  /* Enable I2C GPIO clocks */
  I2Cx_SCL_SDA_GPIO_CLK_ENABLE();

  /* I2Cx SCL and SDA pins configuration ---------------------------*/
  GPIO_InitStruct.Pin = I2Cx_SCL_PIN | I2Cx_SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Alternate  = I2Cx_SCL_SDA_AF;
  HAL_GPIO_Init(I2Cx_SCL_SDA_GPIO_PORT, &GPIO_InitStruct);

  /* Enable the I2Cx peripheral clock */
  I2Cx_CLK_ENABLE();

  /* Force the I2C peripheral clock reset */
  I2Cx_FORCE_RESET();

  /* Release the I2C peripheral clock reset */
  I2Cx_RELEASE_RESET();

  /* Enable and set I2Cx Interrupt to the highest priority */
  // HAL_NVIC_SetPriority(I2Cx_EV_IRQn, 0, 0);
  // HAL_NVIC_EnableIRQ(I2Cx_EV_IRQn);

  /* Enable and set I2Cx Interrupt to the highest priority */
  // HAL_NVIC_SetPriority(I2Cx_ER_IRQn, 0, 0);
  // HAL_NVIC_EnableIRQ(I2Cx_ER_IRQn);
}

#ifdef USE_SPI

/******************************* SPI Routines *********************************/

/**
  * @brief  SPIx Bus initialization
  * @param  None
  * @retval None
  */
static void SPIx_Init(void)
{
  if(HAL_SPI_GetState(&SpiHandle) == HAL_SPI_STATE_RESET)
  {
    /* SPI configuration -----------------------------------------------------*/
    SpiHandle.Instance = DISCOVERY_SPIx;
    SpiHandle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    SpiHandle.Init.Direction = SPI_DIRECTION_2LINES;
    SpiHandle.Init.CLKPhase = SPI_PHASE_1EDGE;
    SpiHandle.Init.CLKPolarity = SPI_POLARITY_LOW;
    SpiHandle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;
    SpiHandle.Init.CRCPolynomial = 7;
    SpiHandle.Init.DataSize = SPI_DATASIZE_8BIT;
    SpiHandle.Init.FirstBit = SPI_FIRSTBIT_MSB;
    SpiHandle.Init.NSS = SPI_NSS_SOFT;
    SpiHandle.Init.TIMode = SPI_TIMODE_DISABLED;
    SpiHandle.Init.Mode = SPI_MODE_MASTER;

    SPIx_MspInit();
    HAL_SPI_Init(&SpiHandle);
  }
}

/**
  * @brief  Sends a Byte through the SPI interface and return the Byte received
  *         from the SPI bus.
  * @param  Byte: Byte send.
  * @retval The received byte value
  */
static uint8_t SPIx_WriteRead(uint8_t Byte)
{
  uint8_t receivedbyte = 0;

  /* Send a Byte through the SPI peripheral */
  /* Read byte from the SPI bus */
  if(HAL_SPI_TransmitReceive(&SpiHandle, (uint8_t*) &Byte, (uint8_t*) &receivedbyte, 1, SpixTimeout) != HAL_OK)
  {
    SPIx_Error();
  }

  return receivedbyte;
}

/**
  * @brief  SPIx error treatment function.
  * @param  None
  * @retval None
  */
static void SPIx_Error(void)
{
  /* De-initialize the SPI communication bus */
  HAL_SPI_DeInit(&SpiHandle);

  /* Re-Initialize the SPI communication bus */
  SPIx_Init();
}

/**
  * @brief  SPI MSP Init.
  * @param  hspi: SPI handle
  * @retval None
  */
static void SPIx_MspInit(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable the SPI peripheral */
  DISCOVERY_SPIx_CLK_ENABLE();

  /* Enable SCK, MOSI and MISO GPIO clocks */
  DISCOVERY_SPIx_GPIO_CLK_ENABLE();

  /* SPI SCK, MOSI, MISO pin configuration */
  GPIO_InitStructure.Pin = (DISCOVERY_SPIx_SCK_PIN | DISCOVERY_SPIx_MISO_PIN | DISCOVERY_SPIx_MOSI_PIN);
  GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStructure.Pull  = GPIO_PULLDOWN;
  GPIO_InitStructure.Speed = GPIO_SPEED_MEDIUM;
  GPIO_InitStructure.Alternate = DISCOVERY_SPIx_AF;
  HAL_GPIO_Init(DISCOVERY_SPIx_GPIO_PORT, &GPIO_InitStructure);
}
#endif



#endif
