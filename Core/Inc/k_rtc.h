/**
  ******************************************************************************
  * @file    k_rtc.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    25-June-2015
  * @brief   Header for k_rtc.c file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __K_RTC_H
#define __K_RTC_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */ 
void     k_CalendarBkupInit(void);
void     k_BkupSaveParameter(uint32_t address, uint32_t data);
uint32_t k_BkupRestoreParameter(uint32_t address);

void k_SetTime  (RTC_TimeTypeDef *Time);
void k_GetTime  (RTC_TimeTypeDef *Time);
void k_SetDate  (RTC_DateTypeDef *Date);
void k_GetDate  (RTC_DateTypeDef *Date);

#ifdef __cplusplus
}
#endif

#endif /*__K_RTC_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
