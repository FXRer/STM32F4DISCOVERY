/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    30-September-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "stm32f4xx.h"
#include "usb_core.h"
#include "usbd_core.h"
#include "usb_conf.h"
#include "usb_hcd_int.h"
#include "usb_dcd_int.h"
#include "usbh_core.h"
#include "global_inc.h"

/** @addtogroup STM32F4xx_StdPeriph_Examples
  * @{
  */

/* External variables ---------------------------------------------------------*/
extern volatile uint16_t one_second_flag;
extern volatile uint8_t button_press;

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

extern USB_OTG_CORE_HANDLE          USB_OTG_Core;
extern USBH_HOST                    USB_Host;

/* Private function prototypes -----------------------------------------------*/
extern void USB_OTG_BSP_TimerIRQ (void);
extern uint32_t USBD_OTG_ISR_Handler (USB_OTG_CORE_HANDLE *pdev);
#ifdef USB_OTG_HS_DEDICATED_EP1_ENABLED
extern uint32_t USBD_OTG_EP1IN_ISR_Handler (USB_OTG_CORE_HANDLE *pdev);
extern uint32_t USBD_OTG_EP1OUT_ISR_Handler (USB_OTG_CORE_HANDLE *pdev);
#endif
/* Private functions ---------------------------------------------------------*/


/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
	while(1)
	{
//		if(wake)
//		{
//			CPU_OFF;
//			while(1);
//		}
	}
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
//		if(wake)
//		{
//			CPU_OFF;
//			while(1);
//		}

	  LCD_String_lc("!ACHTUNG!",4,5,0x00ff,0x0000,2);
	  LCD_String_lc("HARD FAULT",4,6,0x00ff,0x0000,2);
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
//		if(wake)
//		{
//			CPU_OFF;
//			while(1);
//		}


	  LCD_String_lc("! ACHTUNG !",4,9,0xff0000,0x000000,1);
	  LCD_String_lc("MEMORY FAULT",4,10,0xff0000,0x000000,1);
//	  ili9320_String_lc("! ACHTUNG !",4,9,0xff00,0x0000,2);
//	  ili9320_String_lc("MEMORY FAULT",4,10,0xff00,0x0000,2);
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
//		if(wake)
//		{
//			CPU_OFF;
//			while(1);
//		}
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
//		if(wake)
//		{
//			CPU_OFF;
//			while(1);
//		}
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
//void SVC_Handler(void)
//{
//}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
//	if(wake)
//	{
//		CPU_OFF;
//		while(1);
//	}
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
//void PendSV_Handler(void)
//{
//}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
//void SysTick_Handler(void)
//{
//}                                                in main.c
/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles OTG_HS Handler.
  * @param  None
  * @retval None
  */
#ifdef USE_USB_OTG_HS
void OTG_HS_IRQHandler(void)
{
  USBD_OTG_ISR_Handler (&USB_OTG_dev);
}
#endif

#ifdef USE_USB_OTG_FS

void OTG_FS_IRQHandler(void)
{
	//USBD_OTG_ISR_Handler (&USB_OTG_dev);
	if (USB_OTG_IsHostMode(&USB_OTG_Core)) /* ensure that we are in device mode */
	{
		USBH_OTG_ISR_Handler(&USB_OTG_Core);
	}
	else
	{
		USBD_OTG_ISR_Handler(&USB_OTG_Core);
	}
}
#endif

#ifdef USB_OTG_HS_DEDICATED_EP1_ENABLED
/**
  * @brief  This function handles EP1_IN Handler.
  * @param  None
  * @retval None
  */
void OTG_HS_EP1_IN_IRQHandler(void)
{
  USBD_OTG_EP1IN_ISR_Handler (&USB_OTG_dev);
}

/**
  * @brief  This function handles EP1_OUT Handler.
  * @param  None
  * @retval None
  */
void OTG_HS_EP1_OUT_IRQHandler(void)
{
  USBD_OTG_EP1OUT_ISR_Handler (&USB_OTG_dev);
}
#endif

/**
  * @brief  TIM2_IRQHandler
  *         This function handles Timer2 Handler.
  * @param  None
  * @retval None
  */
void TIM3_IRQHandler(void)
{
  USB_OTG_BSP_TimerIRQ();
  //OS_INT_EXIT_EXT();
}

#if 0
/**
  * TIM3 interrupt handler (configured to one second update).
  */
void TIM3_IRQHandler(){
	 if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	 {
		 //GPIO_ToggleBits(LED1_PORT,LED1);
		 /* Signal main loop that one second passed */
		 one_second_flag = 1;
		 //USART_SendData(USART2, 'b');
		 // while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
		 // {}
		 TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	 }
}
#endif
/**
  * @brief  This function handles External lines 15 to 10 interrupt request.
  * @param  None
  * @retval None
  */
//void EXTI15_10_IRQHandler(void)
//{
//	if(EXTI_GetITStatus(EXTI_Line13) != RESET)
//	{
//		button_press = 1;
//		/* Clear the EXTI line 15 pending bit */
//		EXTI_ClearITPendingBit(EXTI_Line13);
//	}
//}
