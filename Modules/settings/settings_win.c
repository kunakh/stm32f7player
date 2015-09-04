/**
  ******************************************************************************
  * @file    settings_win.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    25-June-2015
  * @brief   settings functions
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
#include "settings_res.c"

/** @addtogroup SETTINGS_MODULE
  * @{
  */

/** @defgroup SETTINGS
  * @brief settings routines
  * @{
  */
  
/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void Startup(WM_HWIN hWin, uint16_t xpos, uint16_t ypos);

/* Private typedef -----------------------------------------------------------*/
K_ModuleItem_Typedef  settings_board =
{
  7,
  "system info",
  settings,
  0,
  Startup,
  NULL,
}
;

/* Private defines -----------------------------------------------------------*/
#define ID_WINDOW_0               (GUI_ID_USER + 0x00)

#define ID_TEXT_BOARD             (GUI_ID_USER + 0x02)
#define ID_TEXT_CORE              (GUI_ID_USER + 0x03)
#define ID_TEXT_CPU               (GUI_ID_USER + 0x04)
#define ID_TEXT_VERSION           (GUI_ID_USER + 0x05)
#define ID_TEXT_COPYRIGHT         (GUI_ID_USER + 0x06)
#define ID_TEXT_BOARD_1           (GUI_ID_USER + 0x07)
#define ID_TEXT_BOARD_2           (GUI_ID_USER + 0x0C)

#define ID_TEXT_CORE_1            (GUI_ID_USER + 0x08)
#define ID_TEXT_CPU_1             (GUI_ID_USER + 0x09)
#define ID_TEXT_VERSION_1         (GUI_ID_USER + 0x0A)
#define ID_TEXT_TITLE             (GUI_ID_USER + 0x0B)

#define ID_BUTTON_EXIT            (GUI_ID_USER + 0x20)

#define ID_IMAGE_BOARD            (GUI_ID_USER + 0x21)
#define ID_IMAGE_MCU              (GUI_ID_USER + 0x22)
#define ID_IMAGE_CPU              (GUI_ID_USER + 0x23)
#define ID_IMAGE_FVERSION         (GUI_ID_USER + 0x24)

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static const GUI_WIDGET_CREATE_INFO _aDialog[] = 
{
  { WINDOW_CreateIndirect, "", ID_WINDOW_0,                    0,    0,  480, 272, 0, 0x64, 0 }, 
  { TEXT_CreateIndirect, "System Information", ID_TEXT_TITLE,  160,  20, 300, 40,  0, 0x0,  0 },
  { TEXT_CreateIndirect, "Board", ID_TEXT_BOARD,               57,   90, 80, 20,  0, 0x0,  0 },
  { TEXT_CreateIndirect, "Core", ID_TEXT_CORE,                 170,  90, 80, 20,  0, 0x0,  0 },
  { TEXT_CreateIndirect, "CPU Speed", ID_TEXT_CPU,             260,  90, 80, 20,  0, 0x0,  0 },
  { TEXT_CreateIndirect, "Firm.Ver", ID_TEXT_VERSION,          378,  90, 80, 20,  0, 0x0,  0 },  

  { TEXT_CreateIndirect, " STM32F746G", ID_TEXT_BOARD_1,  30,  190, 90, 20, 0, 0x0, 0 },
  { TEXT_CreateIndirect, "    DISCO",  ID_TEXT_BOARD_2,  40,  205, 80, 20, 0, 0x0, 0 },
  { TEXT_CreateIndirect, " STM32F7", ID_TEXT_CORE_1,     153, 200, 80, 20, 0, 0x0, 0 },
  { TEXT_CreateIndirect, " 200MHz", ID_TEXT_CPU_1,       265, 200, 80, 20, 0, 0x0, 0 },
  /* The demonstration version */
  { TEXT_CreateIndirect, "V1.0.1 ", ID_TEXT_VERSION_1,    385, 200, 80, 20, 0, 0x0, 0 }, 
  
  { TEXT_CreateIndirect, "Copyright (c) STMicroelectronics 2015", ID_TEXT_COPYRIGHT, 260, 253, 240, 20, 0, 0x0, 0 },
};

static WM_HWIN SystemWin;
static WM_HTIMER                  hTimer; 
uint32_t frame = 0;

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Paints exit button
  * @param  hObj: button handle
  * @retval None
  */
static void _OnPaint_exit(BUTTON_Handle hObj) {

  GUI_SetBkColor(FRAMEWIN_GetDefaultClientColor());
  GUI_Clear();

  GUI_SetColor(GUI_STCOLOR_LIGHTBLUE);
  GUI_AA_FillCircle(60, 0, 60);

  GUI_SetBkColor(GUI_STCOLOR_LIGHTBLUE);
  GUI_SetColor(GUI_WHITE);
  GUI_SetFont(&GUI_FontLubalGraph16);
  GUI_DispStringAt("Menu", 15, 13);
}

/**
  * @brief  callback for Exit button
  * @param  pMsg: pointer to data structure of type WM_MESSAGE
  * @retval None
  */
static void _cbButton_exit(WM_MESSAGE * pMsg) {
  switch (pMsg->MsgId) {
    case WM_PAINT:
      _OnPaint_exit(pMsg->hWin);
      break;
    default:
      /* The original callback */
      BUTTON_Callback(pMsg);
      break;
  }
}

static void _cbDialog(WM_MESSAGE * pMsg) {
  WM_HWIN hItem;
  int Id, NCode;

  
  switch (pMsg->MsgId) {
  case WM_INIT_DIALOG:
    
    hItem = BUTTON_CreateEx(420, 0, 60, 60, pMsg->hWin, WM_CF_SHOW, 0, ID_BUTTON_EXIT);
    WM_SetCallback(hItem, _cbButton_exit);
    
    /* Initialization of 'Board : STM324x9I' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_BOARD);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_DARKBLUE    );

    /* Initialization of 'Core: STM32F-4 Series' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_CORE);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_DARKBLUE    );

    /* Initialization of 'CPU Speed : 180MHz' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_CPU);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_DARKBLUE    );

    /* Initialization of 'Firmware Version : 1.0.1' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_VERSION);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_DARKBLUE    ); 

    /* Initialization of 'Board : STM324x9I' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_BOARD_1);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_LIGHTBLUE);

    /* Initialization of 'Board : STM32F746G' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_BOARD_2);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_LIGHTBLUE);    
    
    /* Initialization of 'Core: STM32F-4 Series' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_CORE_1);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_LIGHTBLUE);

    /* Initialization of 'CPU Speed : 180MHz' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_CPU_1);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_LIGHTBLUE);

    /* Initialization of 'Firmware Version : 1.0' */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_VERSION_1);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph16);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_LIGHTBLUE);  
    /* ST Copyright */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_COPYRIGHT);
    TEXT_SetFont(hItem, GUI_FONT_13_ASCII);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_DARKBLUE    );

    /* ST Copyright */
    hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_TITLE);
    TEXT_SetFont(hItem, &GUI_FontLubalGraph20);
    TEXT_SetTextColor(hItem, GUI_STCOLOR_LIGHTBLUE);

    IMAGE_CreateEx(40,  120, 70, 70, pMsg->hWin, WM_CF_SHOW, 0, ID_IMAGE_BOARD);  
    IMAGE_CreateEx(150, 120, 70, 70, pMsg->hWin, WM_CF_SHOW, 0, ID_IMAGE_MCU);
    IMAGE_CreateEx(260, 120, 70, 70, pMsg->hWin, WM_CF_SHOW, 0, ID_IMAGE_CPU);
    IMAGE_CreateEx(370, 120, 70, 70, pMsg->hWin, WM_CF_SHOW, 0, ID_IMAGE_FVERSION);
    
    break;     
    
  case WM_PAINT:    
    GUI_SetColor(GUI_STCOLOR_LIGHTBLUE);
    GUI_AA_DrawRoundedRect(30,  80, 120, 230, 30);
    GUI_AA_DrawRoundedRect(140, 80, 230, 230, 30);
    GUI_AA_DrawRoundedRect(250, 80, 340, 230, 30);
    GUI_AA_DrawRoundedRect(360, 80, 450, 230, 30);
    
    break;     

  case WM_TIMER:
    /* draw */
    
    if(frame < 5)
    {
      WM_RestartTimer(pMsg->Data.v, 25);
      
      hItem = WM_GetDialogItem(pMsg->hWin, ID_IMAGE_BOARD);
      IMAGE_SetBitmap(hItem, open_board[frame]);
      
      hItem = WM_GetDialogItem(pMsg->hWin, ID_IMAGE_MCU);
      IMAGE_SetBitmap(hItem, open_mcu[frame]);
      
      hItem = WM_GetDialogItem(pMsg->hWin, ID_IMAGE_CPU);
      IMAGE_SetBitmap(hItem, open_cpu[frame]);
      
      hItem = WM_GetDialogItem(pMsg->hWin, ID_IMAGE_FVERSION);
      IMAGE_SetBitmap(hItem, open_fversion[frame]);
      
      frame++;
    }
    else
    {
      if(hTimer != 0)
      {
        WM_DeleteTimer(hTimer);
        hTimer = 0;
      }  
    }    
    break;
    
  case WM_DELETE:
    if(hTimer != 0)
    {
      WM_DeleteTimer(hTimer);
      hTimer = 0;
    }    
   
     break;
    
  case WM_NOTIFY_PARENT:
    Id    = WM_GetId(pMsg->hWinSrc);    /* Id of widget */
    NCode = pMsg->Data.v;               /* Notification code */
       
    switch(Id) {
    case ID_BUTTON_EXIT: 
      switch(NCode) {
      case WM_NOTIFICATION_RELEASED:
        if(hTimer != 0)
        {
          WM_DeleteTimer(hTimer);
          hTimer = 0;
        }         
        GUI_EndDialog(pMsg->hWin, 0);

        break;
      }
      break;      
    }
    break;
  default:
    WM_DefaultProc(pMsg);
    break;
  }    
}


/**
  * @brief  Game window Startup
  * @param  hWin: pointer to the parent handle.
  * @param  xpos: X position 
  * @param  ypos: Y position
  * @retval None
  */
static void Startup(WM_HWIN hWin, uint16_t xpos, uint16_t ypos)
{
  frame = 0;
  SystemWin = GUI_CreateDialogBox(_aDialog, GUI_COUNTOF(_aDialog), _cbDialog, hWin, xpos, ypos);
  hTimer = WM_CreateTimer(SystemWin, 0, 100, 0);
}

/**
  * @}
  */

/**
  * @}
  */
  
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
