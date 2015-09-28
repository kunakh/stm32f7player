/*
  ******************************************************************************
  * @file    main.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    25-June-2015
  * @brief   This file provides main program functions
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
 *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private typedef -----------------------------------------------------------*/
typedef struct
{ /* Mail object structure */
  uint32_t var1; /* var1 is a uint32_t */
  uint32_t var2; /* var2 is a uint32_t */
  uint8_t var3; /* var3 is a uint8_t */
} Amail_TypeDef;

/* Private define ------------------------------------------------------------*/
#define blckqSTACK_SIZE   configMINIMAL_STACK_SIZE
#define MAIL_SIZE        (uint32_t) 1

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
osMailQId mailId;

uint32_t ProducerValue1 = 0, ProducerValue2 = 0;
uint8_t ProducerValue3 = 0;
uint32_t ConsumerValue1 = 0, ConsumerValue2 = 0;
uint8_t ConsumerValue3 = 0;

/* Private function prototypes -----------------------------------------------*/

/* Thread function that creates a mail and posts it on a mail queue. */
//static void MailQueueProducer (const void *argument);

/* Thread function that receives mail , remove it  from a mail queue and checks that
it is the expected mail. */
//static void MailQueueConsumer (const void *argument);

void SystemClock_Config(void);
void CPU_CACHE_Enable(void);

/*============================================================================*/
/** @defgroup MAIN_Private_FunctionPrototypes
* @{
*/
static void MPU_Config(void);
//static void GUIThread(void const * argument);
//static void TimerCallback(void const *n);

extern K_ModuleItem_Typedef  video_player_board;
extern K_ModuleItem_Typedef  audio_player_board;
extern K_ModuleItem_Typedef  devices_board;
extern K_ModuleItem_Typedef  games_board;
extern K_ModuleItem_Typedef  gardening_control_board;
extern K_ModuleItem_Typedef  home_alarm_board;
extern K_ModuleItem_Typedef  settings_board;
extern K_ModuleItem_Typedef  audio_recorder_board;
extern K_ModuleItem_Typedef  vnc_server;

osTimerId lcd_timer;

//#pragma location=0x20006024
//uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];

/** @defgroup MAIN_Private_Functions
* @{
*/

/**
* @brief  Main program
* @param  None
* @retval int
*/
int main(void)
{
  /* Configure the MPU attributes as Write Through */
//  MPU_Config();

  /* Enable the CPU Cache */
//  CPU_CACHE_Enable();

  /* Configure the system clock */
//  SystemClock_Config();

  /* STM32F7xx HAL library initialization:
  - Configure the Flash ART accelerator on ITCM interface
  - Configure the Systick to generate an interrupt each 1 msec
  - Set NVIC Group Priority to 4
  - Global MSP (MCU Support Package) initialization
  */
  HAL_Init();

  k_BspInit();

  /* Initialize RTC */
  k_CalendarBkupInit();

  play_init();

#if 0
  /* Create GUI task */
  osThreadDef(GUI_Thread, GUIThread, osPriorityNormal, 0, 2048);
  osThreadCreate (osThread(GUI_Thread), NULL);

  /* Add Modules*/
  k_ModuleInit();

  /* Link modules */
  k_ModuleAdd(&audio_player_board);
  k_ModuleAdd(&video_player_board);
  k_ModuleAdd(&games_board);
  k_ModuleAdd(&audio_recorder_board);
  k_ModuleAdd(&gardening_control_board);
  k_ModuleAdd(&home_alarm_board);
  k_ModuleAdd(&vnc_server);
  k_ModuleAdd(&settings_board);

  /* Initialize GUI */
  GUI_Init();

  WM_MULTIBUF_Enable(1);
  GUI_SetLayerVisEx (1, 0);
  GUI_SelectLayer(0);

  GUI_SetBkColor(GUI_WHITE);
  GUI_Clear();

   /* Set General Graphical proprieties */
  k_SetGuiProfile();

  /* Create Touch screen Timer */
  osTimerDef(TS_Timer, TimerCallback);
  lcd_timer =  osTimerCreate(osTimer(TS_Timer), osTimerPeriodic, (void *)0);

  /* Start the TS Timer */
  osTimerStart(lcd_timer, 100);
#endif // 0
  /* Start scheduler */
  osKernelStart ();

  /* We should never get here as control is now taken by the scheduler */
  for( ;; );
}

#if 0
/**
  * @brief  Start task
  * @param  argument: pointer that is passed to the thread function as start argument.
  * @retval None
  */
static void GUIThread(void const * argument)
{
  /* Initialize Storage Units */
  k_StorageInit();

  /* Demo Startup */
  k_StartUp();

  /* Show the main menu */
  k_InitMenu();

  /* Gui background Task */
  while(1) {
    GUI_Exec(); /* Do the background work ... Update windows etc.) */
    osDelay(20); /* Nothing left to do for the moment ... Idle processing */
  }
}

/**
  * @brief  Timer callbacsk (40 ms)
  * @param  n: Timer index
  * @retval None
  */
static void TimerCallback(void const *n)
{
  k_TouchUpdate();
}
#endif // 0

#ifdef CLK_200MHZ
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 200000000
  *            HCLK(Hz)                       = 200000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 400
  *            PLL_P                          = 2
  *            PLL_Q                          = 8
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 6
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 400;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  HAL_PWREx_EnableOverDrive();

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
}
#else
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 216000000
  *            HCLK(Hz)                       = 216000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 432
  *            PLL_P                          = 2
  *            PLL_Q                          = 9
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 7
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;

  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }

  /* Activate the OverDrive to reach the 216 MHz Frequency */
  ret = HAL_PWREx_EnableOverDrive();
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }

  /* Select PLLSAI output as USB clock source */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 4;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
  ret = HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7);
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }
}
#endif // CLK_200MHZ

/**
  * @brief This function provides accurate delay (in milliseconds) based
  *        on SysTick counter flag.
  * @note This function is declared as __weak to be overwritten in case of other
  *       implementations in user file.
  * @param Delay: specifies the delay duration in milliseconds.
  * @retval None
  */

void HAL_Delay (__IO uint32_t Delay)
{
  while(Delay)
  {
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    {
      Delay--;
    }
  }
}

/**
  * @brief  Configure the MPU attributes as Write Through for SRAM1/2.
  * @note   The Base Address is 0x20010000 since this memory interface is the AXI.
  *         The Region Size is 256KB, it is related to SRAM1 and SRAM2  memory size.
  * @param  None
  * @retval None
  */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;

  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU attributes as WT for SRAM */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x20000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;//MPU_REGION_SIZE_256KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER7;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}


/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
void CPU_CACHE_Enable(void)
{
  /* Enable branch prediction */
  SCB->CCR |= (1 <<18);
  __DSB();

  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

#ifdef USE_FULL_ASSERT
/**
* @brief  assert_failed
*         Reports the name of the source file and the source line number
*         where the assert_param error has occurred.
* @param  File: pointer to the source file name
* @param  Line: assert_param error line source number
* @retval None
*/
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line
  number,ex: printf("Wrong parameters value: file %s on line %d\r\n",
  file, line) */

  /* Infinite loop */
  while (1)
  {}
}

#endif

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  printf("STACK OVERFLOW in task '%s'", pcTaskName);
  Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
  printf("MALLOC FAILED");
  Error_Handler();
}

void *pvPortMalloc(size_t xWantedSize)
{
  return malloc(xWantedSize);
}

void vPortFree(void *pv)
{
  free(pv);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
