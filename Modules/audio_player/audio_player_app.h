/**
  ******************************************************************************
  * @file    audioplayer_app.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    25-June-2015   
  * @brief   header of audio player application file
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
#ifndef __AUDIO_PLAYER_APP_H
#define __AUDIO_PLAYER_APP_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "..\Common\audio_if.h"   
#include "spiritEQ.h"
#include "spiritLdCtrl.h"
   
/* Exported constants --------------------------------------------------------*/
#define MUTE_OFF                      0x00
#define MUTE_ON                       0x01
#define DEFAULT_AUDIO_VOLUME          50

#define CALIBRATION_AUDIOPLAYER_SETTING_BKP   RTC_BKP_DR0
#define CALIBRATION_AUDIOPLAYER_EQU1_BKP      RTC_BKP_DR1
#define CALIBRATION_AUDIOPLAYER_EQU2_BKP      RTC_BKP_DR2
#define CALIBRATION_AUDIOPLAYER_EQU3_BKP      RTC_BKP_DR3
#define CALIBRATION_AUDIOPLAYER_EQU4_BKP      RTC_BKP_DR4
#define CALIBRATION_AUDIOPLAYER_EQU5_BKP      RTC_BKP_DR5
#define CALIBRATION_AUDIOPLAYER_LOUD_BKP      RTC_BKP_DR6


#if (!defined ( __GNUC__ ))  
#define EQUI_DB_TO_PERC(x)       (((int16_t)(x + SPIRIT_EQ_MAX_GAIN_DB) * 100)/40)
#define PERC_TO_EQUI_DB(x)       (((int16_t)(x - 50) * SPIRIT_EQ_MAX_GAIN_DB) / 50)
#define PERC_TO_LDNS_DB(x)       (1 << (SPIRIT_LDCTRL_GAIN_Q_BITS + (int16_t)(50 - x) *  8 / 100 ))  
   
/* Equalizer Band Configuration */
#define SET_BAND_PRMS(band, _fltType, _centerHz, _widthHz, _gainDb) \
  (band)->fltType = _fltType; \
  (band)->centerHz = _centerHz; \
  (band)->widthHz = _widthHz; \
  (band)->gainDb = _gainDb;
#endif
  

typedef enum
{
  AUDIOPLAYER_ERROR_NONE = 0, 
  AUDIOPLAYER_ERROR_IO,
  AUDIOPLAYER_ERROR_HW, 
  AUDIOPLAYER_ERROR_MEM, 
  AUDIOPLAYER_ERROR_FORMAT_NOTSUPPORTED,   
  
}AUDIOPLAYER_ErrorTypdef;

#define AUDIOPLAYER_StateTypdef  OUT_StateTypdef
/* Exported types ------------------------------------------------------------*/


/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Init(uint8_t volume);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Play(uint32_t frequency);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Stop(void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Pause(void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Resume(void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Mute(uint8_t state);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_GetFileInfo(char* file, WAV_InfoTypedef* info);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_SelectFile(char* file);
AUDIOPLAYER_StateTypdef  AUDIOPLAYER_GetState(void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Process(void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_DeInit(void);
uint32_t                 AUDIOPLAYER_GetProgress (void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_SetVolume(uint32_t volume);
uint32_t                 AUDIOPLAYER_GetVolume(void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_NotifyEndOfFile(void);
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_SetPosition(uint32_t position);

#if (!defined ( __GNUC__ ))  
void AUDIO_SetEq(uint8_t BandNum, int16_t NewGainValue);
void AUDIO_SetEqParams(uint32_t loudness_perc);
void AUDIO_SetLoudnessGain(int16_t NewGainValue);
#endif

#ifdef __cplusplus
}
#endif

#endif /*__AUDIO_PLAYER_APP_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
