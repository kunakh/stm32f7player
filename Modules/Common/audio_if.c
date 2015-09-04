/**
  ******************************************************************************
  * @file    audio_if.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    25-June-2015  
  * @brief   Audio common interface
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
#include "audio_if.h"

/** @addtogroup AUDIO_PLAYER_MODULE
  * @{
  */

/** @defgroup AUDIO_APPLICATION
  * @brief audio application routines
  * @{
  */


/* External variables --------------------------------------------------------*/
 static AUDIO_IFTypeDef  AudioIf;
 
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma location=0x20000000
AUDIO_ProcessTypdef haudio;
#elif defined ( __CC_ARM )
AUDIO_ProcessTypdef haudio __attribute__((at(0x20000000))); 
#elif defined ( __GNUC__ ) 
AUDIO_ProcessTypdef haudio __attribute__((section(".RamData1")));
#endif

/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

 
 /**
  * @brief  Register Audio callbacks
  * @param  callbacks
  * @retval None
  */
void AUDIO_IF_RegisterCallbacks(pFunc  tc_cb, 
                                pFunc  ht_cb, 
                                pFunc  err_cb)
{
    AudioIf.TransferComplete_CallBack = tc_cb;
    AudioIf.HalfTransfer_CallBack = ht_cb; 
    AudioIf.Error_CallBack = err_cb;        
}
/**
  * @brief  Manages the DMA Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
  if(AudioIf.TransferComplete_CallBack)
  {
    AudioIf.TransferComplete_CallBack();
  }
}

/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{ 
  if (AudioIf.HalfTransfer_CallBack )
  {
    AudioIf.HalfTransfer_CallBack();
  }
}

/**
  * @brief  Manages the DMA FIFO error interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_Error_CallBack(void)
{
  if(AudioIf.Error_CallBack)
  {
    AudioIf.Error_CallBack();
  }
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
