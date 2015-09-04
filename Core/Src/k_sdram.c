#include "stm32f7xx.h"
#include "stm32f7xx_ll_fmc.h"
#include "stm32f7xx_hal_sdram.h"
#include "stm32746g_discovery_sdram.h"

#include "stm32746g_discovery_qspi.h"

void __SDRAM_Init();
int __QSPI_Init(void);

static void __delay(int d) {
  d *= 1000;
  while(d-- > 0);
}

static void __SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram)
{
  GPIO_InitTypeDef gpio_init_structure;

  /* Enable FMC clock */
  __HAL_RCC_FMC_CLK_ENABLE();

  /* Enable GPIOs clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* Common GPIO configuration */
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_FAST;
  gpio_init_structure.Alternate = GPIO_AF12_FMC;

  /* GPIOC configuration */
  gpio_init_structure.Pin   = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOC, &gpio_init_structure);

  /* GPIOD configuration */
  gpio_init_structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_8 | GPIO_PIN_9 |
                              GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);

  /* GPIOE configuration */
  gpio_init_structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7| GPIO_PIN_8 | GPIO_PIN_9 |\
                              GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |\
                              GPIO_PIN_15;
  HAL_GPIO_Init(GPIOE, &gpio_init_structure);

  /* GPIOF configuration */
  gpio_init_structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2| GPIO_PIN_3 | GPIO_PIN_4 |\
                              GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |\
                              GPIO_PIN_15;
  HAL_GPIO_Init(GPIOF, &gpio_init_structure);

  /* GPIOG configuration */
  gpio_init_structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4| GPIO_PIN_5 | GPIO_PIN_8 |\
                              GPIO_PIN_15;
  HAL_GPIO_Init(GPIOG, &gpio_init_structure);

  /* GPIOH configuration */
  gpio_init_structure.Pin   = GPIO_PIN_3 | GPIO_PIN_5;
  HAL_GPIO_Init(GPIOH, &gpio_init_structure);
}

static int __SDRAM_SendCommand(FMC_SDRAM_TypeDef *Device, FMC_SDRAM_CommandTypeDef *Command, uint32_t Timeout)
{
  __IO uint32_t tmpr = 0;

  /* Set command register */
  tmpr = (uint32_t)((Command->CommandMode)                  |\
                    (Command->CommandTarget)                |\
                    (((Command->AutoRefreshNumber)-1) << 5) |\
                    ((Command->ModeRegisterDefinition) << 9)
                    );

  Device->SDCMR = tmpr;
  int t = 0;

  /* wait until command is send */
  while(HAL_IS_BIT_SET(Device->SDSR, FMC_SDSR_BUSY))
  {
    /* Check for the Timeout */
    if(Timeout != HAL_MAX_DELAY)
    {
      if((Timeout == 0)||((++t > (Timeout * 1000))))
      {
        return HAL_TIMEOUT;
      }
    }

    return HAL_ERROR;
  }
  return HAL_OK;
}

static void __SDRAM_Initialization_sequence(SDRAM_HandleTypeDef *sdramHandle, uint32_t RefreshCount)
{
  FMC_SDRAM_CommandTypeDef Command;

  /* Step 1: Configure a clock configuration enable command */
  Command.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;
  __SDRAM_SendCommand(sdramHandle->Instance, &Command, SDRAM_TIMEOUT);

  /* Step 2: Insert 100 us minimum delay */
  __delay(1);

  /* Step 3: Configure a PALL (precharge all) command */
  Command.CommandMode            = FMC_SDRAM_CMD_PALL;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;
  __SDRAM_SendCommand(sdramHandle->Instance, &Command, SDRAM_TIMEOUT);

  /* Step 4: Configure an Auto Refresh command */
  Command.CommandMode            = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 8;
  Command.ModeRegisterDefinition = 0;
  __SDRAM_SendCommand(sdramHandle->Instance, &Command, SDRAM_TIMEOUT);

  /* Step 5: Program the external memory mode register */
  __IO uint32_t tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |\
                     SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL        |\
                     SDRAM_MODEREG_CAS_LATENCY_2                |\
                     SDRAM_MODEREG_OPERATING_MODE_STANDARD      |\
                     SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

  Command.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = tmpmrd;
  __SDRAM_SendCommand(sdramHandle->Instance, &Command, SDRAM_TIMEOUT);

  /* Step 6: Set the refresh rate counter */
  sdramHandle->Instance->SDRTR |= (RefreshCount<<1);
}

void __SDRAM_Init(void)
{
  SDRAM_HandleTypeDef sdramHandle;
  FMC_SDRAM_TimingTypeDef Timing;

  /* SDRAM device configuration */
  sdramHandle.Instance = FMC_SDRAM_DEVICE;

  /* Timing configuration for 100Mhz as SD clock frequency (System clock is up to 200Mhz) */
  Timing.LoadToActiveDelay    = 2;
  Timing.ExitSelfRefreshDelay = 7;
  Timing.SelfRefreshTime      = 4;
  Timing.RowCycleDelay        = 7;
  Timing.WriteRecoveryTime    = 2;
  Timing.RPDelay              = 2;
  Timing.RCDDelay             = 2;

  sdramHandle.Init.SDBank             = FMC_SDRAM_BANK1;
  sdramHandle.Init.ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_8;
  sdramHandle.Init.RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_12;
  sdramHandle.Init.MemoryDataWidth    = SDRAM_MEMORY_WIDTH;
  sdramHandle.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  sdramHandle.Init.CASLatency         = FMC_SDRAM_CAS_LATENCY_2;
  sdramHandle.Init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  sdramHandle.Init.SDClockPeriod      = SDCLOCK_PERIOD;
  sdramHandle.Init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
  sdramHandle.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0;

  /* SDRAM controller initialization */
  __SDRAM_MspInit(&sdramHandle);

  FMC_SDRAM_Init(sdramHandle.Instance, &(sdramHandle.Init));
  FMC_SDRAM_Timing_Init(sdramHandle.Instance, &Timing, sdramHandle.Init.SDBank);

  /* SDRAM initialization sequence */
  __SDRAM_Initialization_sequence(&sdramHandle, REFRESH_COUNT);
}

static void __QSPI_MspInit(QSPI_HandleTypeDef *hqspi)
{
  GPIO_InitTypeDef gpio_init_structure;

  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable the QuadSPI memory interface clock */
  QSPI_CLK_ENABLE();
  /* Reset the QuadSPI memory interface */
  QSPI_FORCE_RESET();
  QSPI_RELEASE_RESET();
  /* Enable GPIO clocks */
  QSPI_CS_GPIO_CLK_ENABLE();
  QSPI_CLK_GPIO_CLK_ENABLE();
  QSPI_D0_GPIO_CLK_ENABLE();
  QSPI_D1_GPIO_CLK_ENABLE();
  QSPI_D2_GPIO_CLK_ENABLE();
  QSPI_D3_GPIO_CLK_ENABLE();

  /*##-2- Configure peripheral GPIO ##########################################*/
  /* QSPI CS GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_CS_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  gpio_init_structure.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(QSPI_CS_GPIO_PORT, &gpio_init_structure);

  /* QSPI CLK GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_CLK_PIN;
  gpio_init_structure.Pull      = GPIO_NOPULL;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_CLK_GPIO_PORT, &gpio_init_structure);

  /* QSPI D0 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D0_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D0_GPIO_PORT, &gpio_init_structure);

  /* QSPI D1 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D1_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D1_GPIO_PORT, &gpio_init_structure);

  /* QSPI D2 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D2_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D2_GPIO_PORT, &gpio_init_structure);

  /* QSPI D3 GPIO pin configuration  */
  gpio_init_structure.Pin       = QSPI_D3_PIN;
  gpio_init_structure.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D3_GPIO_PORT, &gpio_init_structure);
}

static int __QSPI_MemoryMappedMode(QSPI_HandleTypeDef *h)
{
  QSPI_CommandTypeDef      s_command;
  QSPI_MemoryMappedTypeDef s_mem_mapped_cfg;

  /* Configure the command for the read instruction */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = QUAD_INOUT_FAST_READ_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_4_LINES;
  s_command.AddressSize       = QSPI_ADDRESS_24_BITS;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_4_LINES;
  s_command.DummyCycles       = N25Q128A_DUMMY_CYCLES_READ_QUAD;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Configure the memory mapped mode */
  s_mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_ENABLE;
  s_mem_mapped_cfg.TimeOutPeriod     = 1;

  return HAL_QSPI_MemoryMapped(h, &s_command, &s_mem_mapped_cfg);
}

#include "stm32746g_discovery.h"
#define DUMMY_CLOCK_CYCLES_READ_QUAD         10

extern QSPI_HandleTypeDef QSPIHandle;

static void Error_Handler(void)
{
  BSP_LED_Init(LED1);
  BSP_LED_On(LED1);
  while(1){}
}

#if 0
static void HAL_QSPI_MspInit(QSPI_HandleTypeDef *hqspi)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  static DMA_HandleTypeDef hdma;

  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable the QuadSPI memory interface clock */
  QSPI_CLK_ENABLE();
  /* Reset the QuadSPI memory interface */
  QSPI_FORCE_RESET();
  QSPI_RELEASE_RESET();
  /* Enable GPIO clocks */
  QSPI_CS_GPIO_CLK_ENABLE();
  QSPI_CLK_GPIO_CLK_ENABLE();
  QSPI_D0_GPIO_CLK_ENABLE();
  QSPI_D1_GPIO_CLK_ENABLE();
  QSPI_D2_GPIO_CLK_ENABLE();
  QSPI_D3_GPIO_CLK_ENABLE();
  /* Enable DMA clock */
  QSPI_DMA_CLK_ENABLE();

  /*##-2- Configure peripheral GPIO ##########################################*/
  /* QSPI CS GPIO pin configuration  */
  GPIO_InitStruct.Pin       = QSPI_CS_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(QSPI_CS_GPIO_PORT, &GPIO_InitStruct);

  /* QSPI CLK GPIO pin configuration  */
  GPIO_InitStruct.Pin       = QSPI_CLK_PIN;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_CLK_GPIO_PORT, &GPIO_InitStruct);

  /* QSPI D0 GPIO pin configuration  */
  GPIO_InitStruct.Pin       = QSPI_D0_PIN;
  GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(QSPI_D0_GPIO_PORT, &GPIO_InitStruct);

  /* QSPI D1 GPIO pin configuration  */
  GPIO_InitStruct.Pin       = QSPI_D1_PIN;
  GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(QSPI_D1_GPIO_PORT, &GPIO_InitStruct);

  /* QSPI D2 GPIO pin configuration  */
  GPIO_InitStruct.Pin       = QSPI_D2_PIN;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D2_GPIO_PORT, &GPIO_InitStruct);

  /* QSPI D3 GPIO pin configuration  */
  GPIO_InitStruct.Pin       = QSPI_D3_PIN;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D3_GPIO_PORT, &GPIO_InitStruct);

  /*##-3- Configure the NVIC for QSPI #########################################*/
  /* NVIC configuration for QSPI interrupt */
  HAL_NVIC_SetPriority(QUADSPI_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ(QUADSPI_IRQn);

  /*##-4- Configure the DMA channel ###########################################*/
  /* QSPI DMA channel configuration */
#if 0
  hdma.Init.Channel             = QSPI_DMA_CHANNEL;
  hdma.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma.Init.MemInc              = DMA_MINC_ENABLE;
  hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma.Init.Mode                = DMA_NORMAL;
  hdma.Init.Priority            = DMA_PRIORITY_LOW;
  hdma.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;        /* FIFO mode disabled     */
  hdma.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  hdma.Init.MemBurst            = DMA_MBURST_SINGLE;           /* Memory burst           */
  hdma.Init.PeriphBurst         = DMA_PBURST_SINGLE;           /* Peripheral burst       */
  hdma.Instance                 = QSPI_DMA_INSTANCE;

  __HAL_LINKDMA(hqspi, hdma, hdma);
  HAL_DMA_Init(&hdma);

  /* NVIC configuration for DMA interrupt */
  HAL_NVIC_SetPriority(QSPI_DMA_IRQ, 0x00, 0);
  HAL_NVIC_EnableIRQ(QSPI_DMA_IRQ);
#endif // 0
}

static void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef *hqspi)
{
#if 0
  static DMA_HandleTypeDef hdma;

  /*##-1- Disable the NVIC for QSPI and DMA ##################################*/
  HAL_NVIC_DisableIRQ(QSPI_DMA_IRQ);
  HAL_NVIC_DisableIRQ(QUADSPI_IRQn);

  /*##-2- Disable peripherals ################################################*/
  /* De-configure DMA channel */
  hdma.Instance = QSPI_DMA_INSTANCE;
  HAL_DMA_DeInit(&hdma);
#endif // 0
  /* De-Configure QSPI pins */
  HAL_GPIO_DeInit(QSPI_CS_GPIO_PORT, QSPI_CS_PIN);
  HAL_GPIO_DeInit(QSPI_CLK_GPIO_PORT, QSPI_CLK_PIN);
  HAL_GPIO_DeInit(QSPI_D0_GPIO_PORT, QSPI_D0_PIN);
  HAL_GPIO_DeInit(QSPI_D1_GPIO_PORT, QSPI_D1_PIN);
  HAL_GPIO_DeInit(QSPI_D2_GPIO_PORT, QSPI_D2_PIN);
  HAL_GPIO_DeInit(QSPI_D3_GPIO_PORT, QSPI_D3_PIN);

  /*##-3- Reset peripherals ##################################################*/
  /* Reset the QuadSPI memory interface */
  QSPI_FORCE_RESET();
  QSPI_RELEASE_RESET();

  /* Disable the QuadSPI memory interface clock */
  QSPI_CLK_DISABLE();
}

static HAL_StatusTypeDef HAL_QSPI_DeInit(QSPI_HandleTypeDef *hqspi)
{
  /* Check the QSPI handle allocation */
  if(hqspi == NULL)
  {
    return HAL_ERROR;
  }

  /* Process locked */
  __HAL_LOCK(hqspi);

  /* Disable the QSPI Peripheral Clock */
  __HAL_QSPI_DISABLE(hqspi);

  /* DeInit the low level hardware: GPIO, CLOCK, NVIC... */
  HAL_QSPI_MspDeInit(hqspi);

  /* Set QSPI error code to none */
  hqspi->ErrorCode = HAL_QSPI_ERROR_NONE;

  /* Initialize the QSPI state */
  hqspi->State = HAL_QSPI_STATE_RESET;

  /* Release Lock */
  __HAL_UNLOCK(hqspi);

  return HAL_OK;
}

static void QSPI_WriteEnable(QSPI_HandleTypeDef *hqspi)
{
  QSPI_CommandTypeDef     sCommand;
  QSPI_AutoPollingTypeDef sConfig;

  /* Enable write operations ------------------------------------------ */
  sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCommand.Instruction       = WRITE_ENABLE_CMD;
  sCommand.AddressMode       = QSPI_ADDRESS_NONE;
  sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode          = QSPI_DATA_NONE;
  sCommand.DummyCycles       = 0;
  sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
  sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  sCommand.SIOOMode         = QSPI_SIOO_INST_EVERY_CMD;

  if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure automatic polling mode to wait for write enabling ---- */
  sConfig.Match           = 0x02;
  sConfig.Mask            = 0x02;
  sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
  sConfig.StatusBytesSize = 1;
  sConfig.Interval        = 0x10;
  sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

  sCommand.Instruction    = READ_STATUS_REG_CMD;
  sCommand.DataMode       = QSPI_DATA_1_LINE;

  if (HAL_QSPI_AutoPolling(&QSPIHandle, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function read the SR of the memory and wait the EOP.
  * @param  hqspi: QSPI handle
  * @retval None
  */
static void QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi)
{
  QSPI_CommandTypeDef     sCommand;
  QSPI_AutoPollingTypeDef sConfig;

  /* Configure automatic polling mode to wait for memory ready ------ */
  sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCommand.Instruction       = READ_STATUS_REG_CMD;
  sCommand.AddressMode       = QSPI_ADDRESS_NONE;
  sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode          = QSPI_DATA_1_LINE;
  sCommand.DummyCycles       = 0;
  sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
  sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  sCommand.SIOOMode         = QSPI_SIOO_INST_EVERY_CMD;

  sConfig.Match           = 0x00;
  sConfig.Mask            = 0x01;
  sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
  sConfig.StatusBytesSize = 1;
  sConfig.Interval        = 0x10;
  sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_QSPI_AutoPolling_IT(&QSPIHandle, &sCommand, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function configure the dummy cycles on memory side.
  * @param  hqspi: QSPI handle
  * @retval None
  */
static void QSPI_DummyCyclesCfg(QSPI_HandleTypeDef *hqspi)
{
  QSPI_CommandTypeDef sCommand;
  uint8_t reg;

  /* Read Volatile Configuration register --------------------------- */
  sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCommand.Instruction       = READ_VOL_CFG_REG_CMD;
  sCommand.AddressMode       = QSPI_ADDRESS_NONE;
  sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode          = QSPI_DATA_1_LINE;
  sCommand.DummyCycles       = 0;
  sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
  sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  sCommand.SIOOMode         = QSPI_SIOO_INST_EVERY_CMD;
  sCommand.NbData            = 1;

  if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_QSPI_Receive(&QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  /* Enable write operations ---------------------------------------- */
  QSPI_WriteEnable(&QSPIHandle);

  /* Write Volatile Configuration register (with new dummy cycles) -- */
  sCommand.Instruction = WRITE_VOL_CFG_REG_CMD;
  MODIFY_REG(reg, 0xF0, (DUMMY_CLOCK_CYCLES_READ_QUAD << POSITION_VAL(0xF0)));

  if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_QSPI_Transmit(&QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
}
#endif // end

static uint8_t QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi, uint32_t Timeout)
{
  QSPI_CommandTypeDef     s_command;
  QSPI_AutoPollingTypeDef s_config;

  /* Configure automatic polling mode to wait for memory ready */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = READ_STATUS_REG_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_1_LINE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  s_config.Match           = 0;
  s_config.Mask            = N25Q128A_SR_WIP;
  s_config.MatchMode       = QSPI_MATCH_MODE_AND;
  s_config.StatusBytesSize = 1;
  s_config.Interval        = 0x10;
  s_config.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_QSPI_AutoPolling(hqspi, &s_command, &s_config, Timeout) != HAL_OK)
  {
    return QSPI_ERROR;
  }

  return QSPI_OK;
}

static uint8_t QSPI_ResetMemory(QSPI_HandleTypeDef *hqspi)
{
  QSPI_CommandTypeDef s_command;

  /* Initialize the reset enable command */
  s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  s_command.Instruction       = RESET_ENABLE_CMD;
  s_command.AddressMode       = QSPI_ADDRESS_NONE;
  s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  s_command.DataMode          = QSPI_DATA_NONE;
  s_command.DummyCycles       = 0;
  s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
  s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  /* Send the command */
  if (HAL_QSPI_Command(hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return QSPI_ERROR;
  }

  /* Send the reset memory command */
  s_command.Instruction = RESET_MEMORY_CMD;
  if (HAL_QSPI_Command(hqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return QSPI_ERROR;
  }

  /* Configure automatic polling mode to wait the memory is ready */
  if (QSPI_AutoPollingMemReady(hqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != QSPI_OK)
  {
    return QSPI_ERROR;
  }

  return QSPI_OK;
}

int __QSPI_Init(void)
{
#if 0
  QSPI_CommandTypeDef      sCommand;
  QSPI_MemoryMappedTypeDef sMemMappedCfg;

  CPU_CACHE_Enable();
  __HAL_FLASH_ART_ENABLE();
  SystemClock_Config();

  memset(&QSPIHandle, 0, sizeof(QSPIHandle));
  QSPIHandle.Instance = QUADSPI;
  HAL_QSPI_DeInit(&QSPIHandle);

  /* ClockPrescaler set to 2, so QSPI clock = 216MHz / (2+1) = 72MHz */
  QSPIHandle.Init.ClockPrescaler     = 2;
  QSPIHandle.Init.FifoThreshold      = 4;
  QSPIHandle.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_NONE;
  QSPIHandle.Init.FlashSize          = QSPI_FLASH_SIZE;
  QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
  QSPIHandle.Init.ClockMode          = QSPI_CLOCK_MODE_0;

  if (HAL_QSPI_Init(&QSPIHandle) != HAL_OK)
  {
    Error_Handler();
  }

  sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
  sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
  sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

//  QSPI_AutoPollingMemReady(&QSPIHandle);
//  QSPI_DummyCyclesCfg(&QSPIHandle);

  /* Reading Sequence ------------------------------------------------ */
  sCommand.Instruction = QUAD_OUT_FAST_READ_CMD;
  sCommand.DummyCycles = DUMMY_CLOCK_CYCLES_READ_QUAD;

  sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;

  if (HAL_QSPI_MemoryMapped(&QSPIHandle, &sCommand, &sMemMappedCfg) != HAL_OK)
  {
    Error_Handler();
  }

#else
//  QSPI_HandleTypeDef QSPIHandle = {.Instance = QUADSPI};

  /* Call the DeInit function to reset the driver */
//  if (HAL_QSPI_DeInit(&QSPIHandle) != HAL_OK)
//  {
//    return QSPI_ERROR;
//  }

  /* System level initialization */
  __QSPI_MspInit(&QSPIHandle);

  /* QSPI initialization */
  QSPIHandle.Init.ClockPrescaler     = 1; /* QSPI freq = 216 MHz/(1+1) = 108 Mhz */
  QSPIHandle.Init.FifoThreshold      = 4;
  QSPIHandle.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  QSPIHandle.Init.FlashSize          = POSITION_VAL(N25Q128A_FLASH_SIZE) - 1;
  QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE;
  QSPIHandle.Init.ClockMode          = QSPI_CLOCK_MODE_0;
  QSPIHandle.Init.FlashID            = QSPI_FLASH_ID_1;
  QSPIHandle.Init.DualFlash          = QSPI_DUALFLASH_DISABLE;

  if (HAL_QSPI_Init(&QSPIHandle) != HAL_OK)
  {
    return QSPI_ERROR;
  }

  /* QSPI memory reset */
  if (QSPI_ResetMemory(&QSPIHandle) != QSPI_OK)
  {
    return QSPI_NOT_SUPPORTED;
  }

  /* Configuration of the dummy cycles on QSPI memory side */
//  if (QSPI_DummyCyclesCfg(&QSPIHandle) != QSPI_OK)
//  {
//    return QSPI_NOT_SUPPORTED;
//  }

  if(__QSPI_MemoryMappedMode(&QSPIHandle) != HAL_OK) {
    Error_Handler();
  }

  return 0;
#endif // 0
}
