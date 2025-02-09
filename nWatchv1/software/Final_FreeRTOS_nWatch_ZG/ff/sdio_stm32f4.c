/********************************************************************************/
/*!
	@file			sdio_stm32f4.c
	@author         Nemui Trinomius (http://nemuisan.blog.bai.ne.jp)
    @version        11.00
    @date           2015.03.14
	@brief          SDIO Driver For STM32 F-4  Devices					@n
					Based on STM324xG-EVAL SD-Sample V1.0.2. Thanks!

    @section HISTORY
		2011.10.16	V1.00	Start Here.
		2012.06.01  V2.00	Adopted STM324xG-EVAL SD-Sample V1.0.2.
		2012.09.22  V3.00	Updated Support grater than 32GB Cards.
		2012.10.05  V4.00	Fixed ACMD41 Argument for SDXC(Not UHS-1 mode).
		2013.07.06  V5.00	Fixed over 4GB R/W Problem.
		2013.10.09	V6.00	Integrated with diskio_sdio.c.
		2014.03.17	V7.00	Improved FIFO-Polling mode(can Tx/RX Multiblock).
		2014.11.18	V8.00   Added SD High Speed Mode(optional).
		2015.02.01	V9.00   Added Handling SD High Speed Mode description.
		2015.02.14 V10.00	Optimized global structures to use CCRAM.
		2015.03.14 V11.00	Fixed data corruption on DMA(4-byte unaligned buffer).

    @section LICENSE
		BSD License. See Copyright.txt
*/
/********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "sdio_stm32f4.h"
#include <stm32f4xx_gpio.h>
#include <stm32f4xx_rcc.h>
#include <stm32f4xx_dma.h>
#include <stm32f4xx_sdio.h>
#include <misc.h>

/* check header file version for fool proof */
#if __SDIO_STM32F4_H!= 0x1100
#error "header file version is not correspond!"
#endif

/* Transfer Mode Check */
#if defined(SD_DMA_MODE) && defined(SD_POLLING_MODE)
#error "YOU MUST SELECT EITHER ONE!"
#endif

/* Defines -------------------------------------------------------------------*/
/* Assigned to CoreCoupled-RAM(ccram/stack region) */
#define __ATTR_CCRAM	__attribute__ ((section(".ccram")));

/** 
  * @brief  SDIO Static flags, TimeOut, FIFO Address  
  */
#define SDIO_NULL 						0
#define SDIO_STATIC_FLAGS               ((uint32_t)0x000005FF)
#define SDIO_CMD0TIMEOUT                ((uint32_t)0x000f0000)

/** 
  * @brief  Mask for errors Card Status R1 (OCR Register) 
  */
#define SD_OCR_ADDR_OUT_OF_RANGE        ((uint32_t)0x80000000)
#define SD_OCR_ADDR_MISALIGNED          ((uint32_t)0x40000000)
#define SD_OCR_BLOCK_LEN_ERR            ((uint32_t)0x20000000)
#define SD_OCR_ERASE_SEQ_ERR            ((uint32_t)0x10000000)
#define SD_OCR_BAD_ERASE_PARAM          ((uint32_t)0x08000000)
#define SD_OCR_WRITE_PROT_VIOLATION     ((uint32_t)0x04000000)
#define SD_OCR_LOCK_UNLOCK_FAILED       ((uint32_t)0x01000000)
#define SD_OCR_COM_CRC_FAILED           ((uint32_t)0x00800000)
#define SD_OCR_ILLEGAL_CMD              ((uint32_t)0x00400000)
#define SD_OCR_CARD_ECC_FAILED          ((uint32_t)0x00200000)
#define SD_OCR_CC_ERROR                 ((uint32_t)0x00100000)
#define SD_OCR_GENERAL_UNKNOWN_ERROR    ((uint32_t)0x00080000)
#define SD_OCR_STREAM_READ_UNDERRUN     ((uint32_t)0x00040000)
#define SD_OCR_STREAM_WRITE_OVERRUN     ((uint32_t)0x00020000)
#define SD_OCR_CID_CSD_OVERWRIETE       ((uint32_t)0x00010000)
#define SD_OCR_WP_ERASE_SKIP            ((uint32_t)0x00008000)
#define SD_OCR_CARD_ECC_DISABLED        ((uint32_t)0x00004000)
#define SD_OCR_ERASE_RESET              ((uint32_t)0x00002000)
#define SD_OCR_AKE_SEQ_ERROR            ((uint32_t)0x00000008)
#define SD_OCR_ERRORBITS                ((uint32_t)0xFDFFE008)

/** 
  * @brief  Masks for R6 Response 
  */
#define SD_R6_GENERAL_UNKNOWN_ERROR     ((uint32_t)0x00002000)
#define SD_R6_ILLEGAL_CMD               ((uint32_t)0x00004000)
#define SD_R6_COM_CRC_FAILED            ((uint32_t)0x00008000)

#define SD_VOLTAGE_WINDOW_SD            ((uint32_t)0x80100000)
#define SD_SDXC_XPC_FULLPOWER         	((uint32_t)0x10000000)	/* Nemui added SDXC MAXIMUM Power   */
#define SD_SDXC_S18R_REGULAR_VOLT       ((uint32_t)0x00000000)	/* Nemui added SDXC "NO-1.8V" Drive */
#define SD_HIGH_CAPACITY                ((uint32_t)0x40000000)
#define SD_STD_CAPACITY                 ((uint32_t)0x00000000)
#define SD_CHECK_PATTERN                ((uint32_t)0x000001AA)

#define SD_MAX_VOLT_TRIAL               ((uint32_t)0x0000FFFF)
#define SD_ALLZERO                      ((uint32_t)0x00000000)

#define SD_WIDE_BUS_SUPPORT             ((uint32_t)0x00040000)
#define SD_SINGLE_BUS_SUPPORT           ((uint32_t)0x00010000)
#define SD_CARD_LOCKED                  ((uint32_t)0x02000000)

#define SD_DATATIMEOUT                  ((uint32_t)0xFFFFFFFF)
#define SD_0TO7BITS                     ((uint32_t)0x000000FF)
#define SD_8TO15BITS                    ((uint32_t)0x0000FF00)
#define SD_16TO23BITS                   ((uint32_t)0x00FF0000)
#define SD_24TO31BITS                   ((uint32_t)0xFF000000)
#define SD_MAX_DATA_LENGTH              ((uint32_t)0x01FFFFFF)

#define SD_HALFFIFO                     ((uint32_t)0x00000008)
#define SD_HALFFIFOBYTES                ((uint32_t)0x00000020)

/** 
  * @brief  Command Class Supported 
  */
#define SD_CCCC_LOCK_UNLOCK             ((uint32_t)0x00000080)
#define SD_CCCC_WRITE_PROT              ((uint32_t)0x00000040)
#define SD_CCCC_ERASE                   ((uint32_t)0x00000020)

/** 
  * @brief  Following commands are SD Card Specific commands.
  *         SDIO_APP_CMD should be sent before sending these commands. 
  */
#define SDIO_SEND_IF_COND               ((uint32_t)0x00000008)


/* FatFs Glue */
#define SECTOR_SIZE		512		/* Must be Set "512" in use of SDCARD! 			*/
#define SDIO_DRIVE		0		/* Physical Drive Number set to 0. 				*/
#define SOCKWP			0		/* Write Protect Switch is not Supported.		*/
#define NO_ALIGN4CHK	0		/* 0:Do 4Byte aligned check on DMA Mode(Safe).
								   1:Skip 4Byte aligned check on DMA Mode
								            (Use SingleBurst FIFOMode instead).	*/

/* Variables -----------------------------------------------------------------*/
static uint32_t CardType =  SDIO_STD_CAPACITY_SD_CARD_V1_1;
static uint32_t CSD_Tab[4], CID_Tab[4], RCA = 0;
static uint8_t SDSTATUS_Tab[16];
__IO uint64_t TotalNumberOfBytes = 0;
__IO uint32_t StopCondition = 0;
__IO SD_Error TransferError = SD_OK;
__IO uint32_t TransferEnd = 0, DMAEndOfTransfer = 0;

/* SDCard Structures */
SD_CardInfo SDCardInfo __ATTR_CCRAM;
SD_CardStatus SDCardStatus __ATTR_CCRAM;
SDIO_InitTypeDef SDIO_InitStructure __ATTR_CCRAM;
SDIO_CmdInitTypeDef SDIO_CmdInitStructure __ATTR_CCRAM;
SDIO_DataInitTypeDef SDIO_DataInitStructure __ATTR_CCRAM;
#if defined(SD_DMA_MODE)
DMA_InitTypeDef SDDMA_InitStructure __ATTR_CCRAM;
#if (NO_ALIGN4CHK == 0)
/* If unligned memory address situation,copy dmabuf to aligned by 4-Byte. */
uint8_t dmabuf[SECTOR_SIZE] __attribute__ ((aligned (4)));
#endif
#endif

/* FatFs Glue */
volatile SD_Error Status = SD_OK;
static volatile DSTATUS Stat = STA_NOINIT;	/* Disk status */
static volatile uint32_t Timer1, Timer2;	/* 100Hz decrement timers */

/* Constants -----------------------------------------------------------------*/

/* Function prototypes -------------------------------------------------------*/
static SD_Error CmdError(void);
static SD_Error CmdResp1Error(uint8_t cmd);
static SD_Error CmdResp7Error(void);
static SD_Error CmdResp3Error(void);
static SD_Error CmdResp2Error(void);
static SD_Error CmdResp6Error(uint8_t cmd, uint16_t *prca);
static SD_Error SDEnWideBus(FunctionalState NewState);
static SD_Error IsCardProgramming(uint8_t *pstatus);
static SD_Error FindSCR(uint16_t rca, uint32_t *pscr);
uint8_t convert_from_bytes_to_power_of_two(uint16_t NumberOfBytes);
#if defined(SD_DMA_MODE)
static void SD_LowLevel_DMA_TxConfig(uint32_t *BufferSRC, uint32_t BufferSize);
static void SD_LowLevel_DMA_RxConfig(uint32_t *BufferDST, uint32_t BufferSize);
#endif
/* Functions -----------------------------------------------------------------*/
























//DSTATUS TM_FATFS_SD_SDIO_disk_initialize(void) {
//#if FATFS_USE_DETECT_PIN > 0 || FATFS_USE_WRITEPROTECT_PIN > 0
//	GPIO_InitTypeDef GPIO_InitStruct;
//#endif
//	NVIC_InitTypeDef NVIC_InitStructure;
//
//#if FATFS_USE_DETECT_PIN > 0
//	RCC_AHB1PeriphClockCmd(FATFS_USE_DETECT_PIN_RCC, ENABLE);
//
//	GPIO_InitStruct.GPIO_Pin = FATFS_USE_DETECT_PIN_PIN;
//	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
//	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
//	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
//	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
//
//	GPIO_Init(FATFS_USE_DETECT_PIN_PORT, &GPIO_InitStruct);
//#endif
//
//#if FATFS_USE_WRITEPROTECT_PIN > 0
//
//	RCC_AHB1PeriphClockCmd(FATFS_USE_WRITEPROTECT_PIN_RCC, ENABLE);
//
//	GPIO_InitStruct.GPIO_Pin = FATFS_USE_WRITEPROTECT_PIN_PIN;
//	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
//	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
//	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
//	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
//
//	GPIO_Init(FATFS_USE_WRITEPROTECT_PIN_PORT, &GPIO_InitStruct);
//#endif
//
//	// Configure the NVIC Preemption Priority Bits
//	NVIC_PriorityGroupConfig (NVIC_PriorityGroup_1);
//	NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
//	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//	NVIC_Init (&NVIC_InitStructure);
//	NVIC_InitStructure.NVIC_IRQChannel = SD_SDIO_DMA_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
//	NVIC_Init (&NVIC_InitStructure);
//
//	//SD_LowLevel_DeInit();
//	//SD_LowLevel_Init();
//
//	//Check disk initialized
//	if (SD_Init() == SD_OK) {
//		TM_FATFS_SD_SDIO_Stat &= ~STA_NOINIT;	/* Clear STA_NOINIT flag */
//	} else {
//		TM_FATFS_SD_SDIO_Stat |= STA_NOINIT;
//	}
//	//Check write protected
//	if (!TM_FATFS_SDIO_WriteEnabled()) {
//		TM_FATFS_SD_SDIO_Stat |= STA_PROTECT;
//	} else {
//		TM_FATFS_SD_SDIO_Stat &= ~STA_PROTECT;
//	}
//
//	return TM_FATFS_SD_SDIO_Stat;
//}

//DSTATUS TM_FATFS_SD_SDIO_disk_status(void) {
//	if (SD_Detect() != SD_PRESENT) {
//		return STA_NOINIT;
//	}
//
//	if (!TM_FATFS_SDIO_WriteEnabled()) {
//		TM_FATFS_SD_SDIO_Stat |= STA_PROTECT;
//	} else {
//		TM_FATFS_SD_SDIO_Stat &= ~STA_PROTECT;
//	}
//
//	return TM_FATFS_SD_SDIO_Stat;
//}
//
//DRESULT TM_FATFS_SD_SDIO_disk_read(BYTE *buff, DWORD sector, UINT count) {
//	SD_Error status = SD_OK;
//
//	if ((TM_FATFS_SD_SDIO_Stat & STA_NOINIT)) {
//		return RES_NOTRDY;
//	}
//
//	SD_ReadMultiBlocks(buff, sector << 9, 512, count);
//
//	/* Check if the Transfer is finished */
//	status =  SD_WaitReadOperation();
//	while(SD_GetStatus() != SD_TRANSFER_OK);
//
//	if (status == SD_OK) {
//		return RES_OK;
//	}
//	return RES_ERROR;
//}
//
//DRESULT TM_FATFS_SD_SDIO_disk_write(BYTE *buff, DWORD sector, UINT count) {
//	SD_Error status = SD_OK;
//
//	if (!TM_FATFS_SDIO_WriteEnabled()) {
//		return RES_WRPRT;
//	}
//
//	SD_WriteMultiBlocks((BYTE *)buff, sector << 9, 512, count);
//
//	/* Check if the Transfer is finished */
//	status = SD_WaitWriteOperation();
//	while(SD_GetStatus() != SD_TRANSFER_OK);
//
//	if (status == SD_OK) {
//		return RES_OK;
//	}
//	return RES_ERROR;
//}

//DRESULT TM_FATFS_SD_SDIO_disk_ioctl(BYTE cmd, char *buff) {
//	switch (cmd) {
//		case GET_SECTOR_SIZE :     // Get R/W sector size (WORD)
//			*(WORD * )buff = 512;
//		break;
//		case GET_BLOCK_SIZE :      // Get erase block size in unit of sector (DWORD)
//			*(DWORD * )buff = 32;
//		break;
//		case CTRL_SYNC :
//		case CTRL_ERASE_SECTOR :
//		break;
//	}
//
//	return RES_OK;
//}
























/**************************************************************************/
/*! 
    @brief DeInitializes the SDIO interface.
	@param  None
    @retval : None
*/
/**************************************************************************/
void SD_DeInit(void)
{ 
	GPIO_InitTypeDef  GPIO_InitStructure;

	/*!< Disable SDIO Clock */
	SDIO_ClockCmd(DISABLE);

	/*!< Set Power State to OFF */
	SDIO_SetPowerState(SDIO_PowerState_OFF);

	/*!< DeInitializes the SDIO peripheral */
	SDIO_DeInit();

	/* Disable the SDIO APB2 Clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, DISABLE);

	GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_MCO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_MCO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_MCO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_MCO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_MCO);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource2, GPIO_AF_MCO);

	/* Configure PC.08, PC.09, PC.10, PC.11 pins: D0, D1, D2, D3 pins */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure PD.02 CMD line */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* Configure PC.12 pin: CLK pin */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}


/**************************************************************************/
/*! 
    @brief  Initializes the SD Card and put it into StandBy State (Ready for data 
            transfer).
	@param  None
    @retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	NVIC_InitTypeDef  NVIC_InitStructure;
	SD_Error errorstatus = SD_OK;

	/* SDIO Peripheral Low Level Init */
	/* GPIOC and GPIOD Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD | SD_DETECT_GPIO_CLK, ENABLE);

	GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource2, GPIO_AF_SDIO);

	/* Configure PC.08, PC.09, PC.10, PC.11 pins: D0, D1, D2, D3 pins */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* Configure PD.02 CMD line */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* Configure PC.12 pin: CLK pin */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/*!< Configure SD_SPI_DETECT_PIN pin: SD Card detect pin */
#ifdef SDIO_INS_DETECT
	GPIO_InitStructure.GPIO_Pin = SD_DETECT_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(SD_DETECT_GPIO_PORT, &GPIO_InitStructure);
#endif

	/*
	 *NVIC_PriorityGroup_0: 0 Pre-emption priorities, 16 Sub-priorities
	 *NVIC_PriorityGroup_1: 2 Pre-emption priorities, 8 Sub-priorities
	 *NVIC_PriorityGroup_2: 4 Pre-emption priorities, 4 Sub-priorities
	 *NVIC_PriorityGroup_3: 8 Pre-emption priorities, 2 Sub-priorities
	 *NVIC_PriorityGroup_4: 16 Pre-emption priorities, 0 Sub-priorities
	 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

	/* Enable the SDIO Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = SD_SDIO_DMA_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_Init(&NVIC_InitStructure);  

	/* Enable the SDIO APB2 Clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, ENABLE);

#if defined(SD_DMA_MODE)
	/* Enable the DMA Clock */
	RCC_AHB1PeriphClockCmd(SD_SDIO_DMA_CLK, ENABLE);
	/* Initialize SDDMA Structure */
	SDDMA_InitStructure.DMA_Channel 			= SD_SDIO_DMA_CHANNEL;
	SDDMA_InitStructure.DMA_PeripheralBaseAddr 	= (uint32_t)SDIO_FIFO_ADDRESS;
	SDDMA_InitStructure.DMA_Memory0BaseAddr 	= 0;
	SDDMA_InitStructure.DMA_DIR 				= DMA_DIR_PeripheralToMemory;
	SDDMA_InitStructure.DMA_BufferSize 			= 0;
	SDDMA_InitStructure.DMA_PeripheralInc 		= DMA_PeripheralInc_Disable;
	SDDMA_InitStructure.DMA_MemoryInc 			= DMA_MemoryInc_Enable;
#if (NO_ALIGN4CHK == 0)
	SDDMA_InitStructure.DMA_PeripheralDataSize 	= DMA_PeripheralDataSize_Word;
	SDDMA_InitStructure.DMA_MemoryDataSize 		= DMA_MemoryDataSize_Word;
	SDDMA_InitStructure.DMA_Mode 				= DMA_Mode_Normal;
	SDDMA_InitStructure.DMA_Priority 			= DMA_Priority_VeryHigh;
	SDDMA_InitStructure.DMA_FIFOMode 			= DMA_FIFOMode_Enable;
	SDDMA_InitStructure.DMA_FIFOThreshold 		= DMA_FIFOThreshold_Full;
	SDDMA_InitStructure.DMA_MemoryBurst 		= DMA_MemoryBurst_INC4;
	SDDMA_InitStructure.DMA_PeripheralBurst 	= DMA_PeripheralBurst_INC4;
#else /* (NO_ALIGN4CHK == 1) */
	SDDMA_InitStructure.DMA_PeripheralDataSize 	= DMA_PeripheralDataSize_Word;
	SDDMA_InitStructure.DMA_MemoryDataSize 		= DMA_MemoryDataSize_Byte;
	SDDMA_InitStructure.DMA_Mode 				= DMA_Mode_Normal;
	SDDMA_InitStructure.DMA_Priority 			= DMA_Priority_VeryHigh;
	SDDMA_InitStructure.DMA_FIFOMode 			= DMA_FIFOMode_Enable;
	SDDMA_InitStructure.DMA_FIFOThreshold 		= DMA_FIFOThreshold_HalfFull;
	SDDMA_InitStructure.DMA_MemoryBurst 		= DMA_MemoryBurst_Single;
	SDDMA_InitStructure.DMA_PeripheralBurst 	= DMA_PeripheralBurst_INC4;
#endif
#endif
	/* End of LowLevel Init */


	SDIO_DeInit();

	errorstatus = SD_PowerON();

	if (errorstatus != SD_OK)
	{
		/*!< CMD Response TimeOut (wait for CMDSENT flag) */
		return(errorstatus);
	}

	errorstatus = SD_InitializeCards();

	if (errorstatus != SD_OK)
	{
		/*!< CMD Response TimeOut (wait for CMDSENT flag) */
		return(errorstatus);
	}

	/*!< Configure the SDIO peripheral */
	/*!< SDIOCLK = HCLK, SDIO_CK = HCLK/(2 + SDIO_TRANSFER_CLK_DIV) */
	/*!< on STM32F4xx devices, SDIOCLK is fixed to 48MHz */  
	SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
	SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
	SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
	SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
	SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_1b;
	SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
	SDIO_Init(&SDIO_InitStructure);

	/*----------------- Read CSD/CID MSD registers ------------------*/
	if (errorstatus == SD_OK)
	{
		errorstatus = SD_GetCardInfo(&SDCardInfo);
	}

	/*----------------- Select Card --------------------------------*/
	if (errorstatus == SD_OK)
	{
		errorstatus = SD_SelectDeselect((uint32_t) (SDCardInfo.RCA << 16));
	}

	/*----------------- Enable SDC 4BitMode --------------------------------*/
	if (errorstatus == SD_OK)
	{
		errorstatus = SD_EnableWideBusOperation(SDIO_BusWide_4b);
	}

#ifdef SD_HS_MODE
 	/*----------------- Enable HighSpeedMode --------------------------------*/
	#warning "Enable SD High Speed mode!"
	if (errorstatus == SD_OK)
	{
		errorstatus = SD_HighSpeed();
	}
#endif

	return(errorstatus);
}

  
/**************************************************************************/
/*! 
    @brief  Gets the cuurent sd card data transfer status.
	@param  None
    @retval SDTransferState: Data Transfer state.
			This value can be: 
			- SD_TRANSFER_OK: No data transfer is acting
			- SD_TRANSFER_BUSY: Data transfer is acting
*/
/**************************************************************************/
SDTransferState SD_GetStatus(void)
{
	SDCardState cardstate =  SD_CARD_TRANSFER;

	cardstate = SD_GetState();

	if (cardstate == SD_CARD_TRANSFER)
	{
		return(SD_TRANSFER_OK);
	}
	else if(cardstate == SD_CARD_ERROR)
	{
		return (SD_TRANSFER_ERROR);
	}
	else
	{
		return(SD_TRANSFER_BUSY);
	}
}


/**************************************************************************/
/*! 
    @brief  Returns the current card's state.
	@param  None
    @retval SDCardState: SD Card Error or SD Card Current State.
*/
/**************************************************************************/
SDCardState SD_GetState(void)
{
	uint32_t resp1 = 0;

	if(SD_Detect()== SD_PRESENT)
	{
		if (SD_SendStatus(&resp1) != SD_OK)
		{
			return SD_CARD_ERROR;
		}
		else
		{
			return (SDCardState)((resp1 >> 9) & 0x0F);
		}
	}
	else
	{
		return SD_CARD_ERROR;
	}
}


/**************************************************************************/
/*! 
    @brief  Detect if SD card is correctly plugged in the memory slot.
	@param  None
    @retval Return if SD is detected or not
*/
/**************************************************************************/
uint8_t SD_Detect(void)
{
	__IO uint8_t status = SD_PRESENT;

	/*!< Check GPIO to detect SD */
#ifdef SDIO_INS_DETECT

	if(SD_DETECT_GPIO_PORT->IDR & SD_DETECT_PIN)
	{
		status = SD_NOT_PRESENT;
	}
#endif
	return status;
}


/**************************************************************************/
/*! 
    @brief  Enquires cards about their operating voltage and configures 
			clock controls.
	@param  None
    @retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_PowerON(void)
{
	__IO SD_Error errorstatus = SD_OK;
	uint32_t response = 0, count = 0, validvoltage = 0;
	uint32_t SDType = SD_STD_CAPACITY;

	/*!< Power ON Sequence -----------------------------------------------------*/
	/*!< Configure the SDIO peripheral */
	/*!< SDIOCLK = HCLK, SDIO_CK = HCLK/(2 + SDIO_INIT_CLK_DIV) */
	/*!< on STM32F4xx devices, SDIOCLK is fixed to 48MHz */
	/*!< SDIO_CK for initialization should not exceed 400 KHz */  
	SDIO_InitStructure.SDIO_ClockDiv = SDIO_INIT_CLK_DIV;
	SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
	SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
	SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
	SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_1b;
	SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
	SDIO_Init(&SDIO_InitStructure);

	/*!< Set Power State to ON */
	SDIO_SetPowerState(SDIO_PowerState_ON);

	/*!< Enable SDIO Clock */
	SDIO_ClockCmd(ENABLE);

	/*!< CMD0: GO_IDLE_STATE ---------------------------------------------------*/
	/*!< No CMD response required */
	SDIO_CmdInitStructure.SDIO_Argument = 0x0;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_GO_IDLE_STATE;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_No;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdError();

	if (errorstatus != SD_OK)
	{
		/*!< CMD Response TimeOut (wait for CMDSENT flag) */
		return(errorstatus);
	}

	/*!< CMD8: SEND_IF_COND ----------------------------------------------------*/
	/*!< Send CMD8 to verify SD card interface operating condition */
	/*!< Argument: - [31:12]: Reserved (shall be set to '0')
			   - [11:8]: Supply Voltage (VHS) 0x1 (Range: 2.7-3.6 V)
			   - [7:0]: Check Pattern (recommended 0xAA) */
	/*!< CMD Response: R7 */
	SDIO_CmdInitStructure.SDIO_Argument = SD_CHECK_PATTERN;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SEND_IF_COND;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp7Error();

	if (errorstatus == SD_OK)
	{
		CardType = SDIO_STD_CAPACITY_SD_CARD_V2_0; /*!< SD Card 2.0 */
		SDType = SD_HIGH_CAPACITY;
	}
	else
	{
		/*!< CMD55 */
		SDIO_CmdInitStructure.SDIO_Argument = 0x00;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdResp1Error(SD_CMD_APP_CMD);
	}
	/*!< CMD55 */
	SDIO_CmdInitStructure.SDIO_Argument = 0x00;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);
	errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

	/*!< If errorstatus is Command TimeOut, it is a MMC card */
	/*!< If errorstatus is SD_OK it is a SD card: SD card 2.0 (voltage range mismatch)
	 or SD card 1.x */
	if (errorstatus == SD_OK)
	{
		/*!< SD CARD */
		/*!< Send ACMD41 SD_APP_OP_COND with Argument 0x80100000 */
		while ((!validvoltage) && (count < SD_MAX_VOLT_TRIAL))
		{

			/*!< SEND CMD55 APP_CMD with RCA as 0 */
			SDIO_CmdInitStructure.SDIO_Argument = 0x00;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);

			errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

			if (errorstatus != SD_OK)
			{
			return(errorstatus);
			}
			SDIO_CmdInitStructure.SDIO_Argument = SD_VOLTAGE_WINDOW_SD  	| \
												  SD_SDXC_XPC_FULLPOWER 	| \
												  SD_SDXC_S18R_REGULAR_VOLT	| \
												  SDType;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SD_APP_OP_COND;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;/////////////////
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);

			errorstatus = CmdResp3Error();
			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}

			response = SDIO_GetResponse(SDIO_RESP1);
			validvoltage = (((response >> 31) == 1) ? 1 : 0);
			count++;
		}
		
		if (count >= SD_MAX_VOLT_TRIAL)
		{
		  errorstatus = SD_INVALID_VOLTRANGE;
		  return(errorstatus);
		}

		if (response &= SD_HIGH_CAPACITY)
		{
			CardType = SDIO_HIGH_CAPACITY_SD_CARD;
		}

	}/*!< else MMC Card */

	return(errorstatus);
}


/**************************************************************************/
/*! 
    @brief  Turns the SDIO output signals off.
	@param  None
    @retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_PowerOFF(void)
{
	SD_Error errorstatus = SD_OK;

	/*!< Set Power State to OFF */
	SDIO_SetPowerState(SDIO_PowerState_OFF);

	return(errorstatus);
}


/**************************************************************************/
/*! 
    @brief  Intialises all cards or single card as the case may be Card(s) come 
			into standby state.
	@param  None
    @retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_InitializeCards(void)
{
	SD_Error errorstatus = SD_OK;
	uint16_t rca = 0x01;

	if (SDIO_GetPowerState() == SDIO_PowerState_OFF)
	{
		errorstatus = SD_REQUEST_NOT_APPLICABLE;
		return(errorstatus);
	}

	if (SDIO_SECURE_DIGITAL_IO_CARD != CardType)
	{
		/*!< Send CMD2 ALL_SEND_CID */
		SDIO_CmdInitStructure.SDIO_Argument = 0x0;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_ALL_SEND_CID;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Long;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);

		errorstatus = CmdResp2Error();

		if (SD_OK != errorstatus)
		{
		  return(errorstatus);
		}

		CID_Tab[0] = SDIO_GetResponse(SDIO_RESP1);
		CID_Tab[1] = SDIO_GetResponse(SDIO_RESP2);
		CID_Tab[2] = SDIO_GetResponse(SDIO_RESP3);
		CID_Tab[3] = SDIO_GetResponse(SDIO_RESP4);
	}
	if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) ||  (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) ||  (SDIO_SECURE_DIGITAL_IO_COMBO_CARD == CardType)
	  ||  (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
	{
		/*!< Send CMD3 SET_REL_ADDR with argument 0 */
		/*!< SD Card publishes its RCA. */
		SDIO_CmdInitStructure.SDIO_Argument = 0x00;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_REL_ADDR;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);

		errorstatus = CmdResp6Error(SD_CMD_SET_REL_ADDR, &rca);

		if (SD_OK != errorstatus)
		{
		  return(errorstatus);
		}
	}

	if (SDIO_SECURE_DIGITAL_IO_CARD != CardType)
	{
		RCA = rca;

		/*!< Send CMD9 SEND_CSD with argument as card's RCA */
		SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)(rca << 16);
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SEND_CSD;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Long;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);

		errorstatus = CmdResp2Error();

		if (SD_OK != errorstatus)
		{
		  return(errorstatus);
		}

		CSD_Tab[0] = SDIO_GetResponse(SDIO_RESP1);
		CSD_Tab[1] = SDIO_GetResponse(SDIO_RESP2);
		CSD_Tab[2] = SDIO_GetResponse(SDIO_RESP3);
		CSD_Tab[3] = SDIO_GetResponse(SDIO_RESP4);
	}

	errorstatus = SD_OK; /*!< All cards get intialized */

	return(errorstatus);
}


/**************************************************************************/
/*! 
    @brief  Returns information about specific card.
	@param  cardinfo: pointer to a SD_CardInfo structure that contains all SD card 
			information.
    @retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_GetCardInfo(SD_CardInfo *cardinfo)
{
	SD_Error errorstatus = SD_OK;
	uint8_t tmp = 0;

	cardinfo->CardType = (uint8_t)CardType;
	cardinfo->RCA = (uint16_t)RCA;

	/*!< Byte 0 */
	tmp = (uint8_t)((CSD_Tab[0] & 0xFF000000) >> 24);
	cardinfo->SD_csd.CSDStruct = (tmp & 0xC0) >> 6;
	cardinfo->SD_csd.SysSpecVersion = (tmp & 0x3C) >> 2;
	cardinfo->SD_csd.Reserved1 = tmp & 0x03;

	/*!< Byte 1 */
	tmp = (uint8_t)((CSD_Tab[0] & 0x00FF0000) >> 16);
	cardinfo->SD_csd.TAAC = tmp;

	/*!< Byte 2 */
	tmp = (uint8_t)((CSD_Tab[0] & 0x0000FF00) >> 8);
	cardinfo->SD_csd.NSAC = tmp;

	/*!< Byte 3 */
	tmp = (uint8_t)(CSD_Tab[0] & 0x000000FF);
	cardinfo->SD_csd.MaxBusClkFrec = tmp;

	/*!< Byte 4 */
	tmp = (uint8_t)((CSD_Tab[1] & 0xFF000000) >> 24);
	cardinfo->SD_csd.CardComdClasses = tmp << 4;

	/*!< Byte 5 */
	tmp = (uint8_t)((CSD_Tab[1] & 0x00FF0000) >> 16);
	cardinfo->SD_csd.CardComdClasses |= (tmp & 0xF0) >> 4;
	cardinfo->SD_csd.RdBlockLen = tmp & 0x0F;

	/*!< Byte 6 */
	tmp = (uint8_t)((CSD_Tab[1] & 0x0000FF00) >> 8);
	cardinfo->SD_csd.PartBlockRead = (tmp & 0x80) >> 7;
	cardinfo->SD_csd.WrBlockMisalign = (tmp & 0x40) >> 6;
	cardinfo->SD_csd.RdBlockMisalign = (tmp & 0x20) >> 5;
	cardinfo->SD_csd.DSRImpl = (tmp & 0x10) >> 4;
	cardinfo->SD_csd.Reserved2 = 0; /*!< Reserved */

	if ((CardType == SDIO_STD_CAPACITY_SD_CARD_V1_1) || (CardType == SDIO_STD_CAPACITY_SD_CARD_V2_0))
	{
		cardinfo->SD_csd.DeviceSize = (tmp & 0x03) << 10;

		/*!< Byte 7 */
		tmp = (uint8_t)(CSD_Tab[1] & 0x000000FF);
		cardinfo->SD_csd.DeviceSize |= (tmp) << 2;

		/*!< Byte 8 */
		tmp = (uint8_t)((CSD_Tab[2] & 0xFF000000) >> 24);
		cardinfo->SD_csd.DeviceSize |= (tmp & 0xC0) >> 6;

		cardinfo->SD_csd.MaxRdCurrentVDDMin = (tmp & 0x38) >> 3;
		cardinfo->SD_csd.MaxRdCurrentVDDMax = (tmp & 0x07);

		/*!< Byte 9 */
		tmp = (uint8_t)((CSD_Tab[2] & 0x00FF0000) >> 16);
		cardinfo->SD_csd.MaxWrCurrentVDDMin = (tmp & 0xE0) >> 5;
		cardinfo->SD_csd.MaxWrCurrentVDDMax = (tmp & 0x1C) >> 2;
		cardinfo->SD_csd.DeviceSizeMul = (tmp & 0x03) << 1;
		/*!< Byte 10 */
		tmp = (uint8_t)((CSD_Tab[2] & 0x0000FF00) >> 8);
		cardinfo->SD_csd.DeviceSizeMul |= (tmp & 0x80) >> 7;

		cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1) ;
		cardinfo->CardCapacity *= (1 << (cardinfo->SD_csd.DeviceSizeMul + 2));
		cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);
		cardinfo->CardCapacity *= cardinfo->CardBlockSize;
	}
	else if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		/*!< Byte 7 */
		tmp = (uint8_t)(CSD_Tab[1] & 0x000000FF);
		cardinfo->SD_csd.DeviceSize = (tmp & 0x3F) << 16;

		/*!< Byte 8 */
		tmp = (uint8_t)((CSD_Tab[2] & 0xFF000000) >> 24);

		cardinfo->SD_csd.DeviceSize |= (tmp << 8);

		/*!< Byte 9 */
		tmp = (uint8_t)((CSD_Tab[2] & 0x00FF0000) >> 16);

		cardinfo->SD_csd.DeviceSize |= (tmp);

		/*!< Byte 10 */
		tmp = (uint8_t)((CSD_Tab[2] & 0x0000FF00) >> 8);
		/* nemui fixed due to SD2.00 Capacity fomula is Size = (C_SIZE+1)�~2^19 */
		cardinfo->CardCapacity = ((uint64_t)cardinfo->SD_csd.DeviceSize + 1) * 512 * 1024;
		cardinfo->CardBlockSize = 512;    
	}


	cardinfo->SD_csd.EraseGrSize = (tmp & 0x40) >> 6;
	cardinfo->SD_csd.EraseGrMul = (tmp & 0x3F) << 1;

	/*!< Byte 11 */
	tmp = (uint8_t)(CSD_Tab[2] & 0x000000FF);
	cardinfo->SD_csd.EraseGrMul |= (tmp & 0x80) >> 7;
	cardinfo->SD_csd.WrProtectGrSize = (tmp & 0x7F);

	/*!< Byte 12 */
	tmp = (uint8_t)((CSD_Tab[3] & 0xFF000000) >> 24);
	cardinfo->SD_csd.WrProtectGrEnable = (tmp & 0x80) >> 7;
	cardinfo->SD_csd.ManDeflECC = (tmp & 0x60) >> 5;
	cardinfo->SD_csd.WrSpeedFact = (tmp & 0x1C) >> 2;
	cardinfo->SD_csd.MaxWrBlockLen = (tmp & 0x03) << 2;

	/*!< Byte 13 */
	tmp = (uint8_t)((CSD_Tab[3] & 0x00FF0000) >> 16);
	cardinfo->SD_csd.MaxWrBlockLen |= (tmp & 0xC0) >> 6;
	cardinfo->SD_csd.WriteBlockPaPartial = (tmp & 0x20) >> 5;
	cardinfo->SD_csd.Reserved3 = 0;
	cardinfo->SD_csd.ContentProtectAppli = (tmp & 0x01);

	/*!< Byte 14 */
	tmp = (uint8_t)((CSD_Tab[3] & 0x0000FF00) >> 8);
	cardinfo->SD_csd.FileFormatGrouop = (tmp & 0x80) >> 7;
	cardinfo->SD_csd.CopyFlag = (tmp & 0x40) >> 6;
	cardinfo->SD_csd.PermWrProtect = (tmp & 0x20) >> 5;
	cardinfo->SD_csd.TempWrProtect = (tmp & 0x10) >> 4;
	cardinfo->SD_csd.FileFormat = (tmp & 0x0C) >> 2;
	cardinfo->SD_csd.ECC = (tmp & 0x03);

	/*!< Byte 15 */
	tmp = (uint8_t)(CSD_Tab[3] & 0x000000FF);
	cardinfo->SD_csd.CSD_CRC = (tmp & 0xFE) >> 1;
	cardinfo->SD_csd.Reserved4 = 1;


	/*!< Byte 0 */
	tmp = (uint8_t)((CID_Tab[0] & 0xFF000000) >> 24);
	cardinfo->SD_cid.ManufacturerID = tmp;

	/*!< Byte 1 */
	tmp = (uint8_t)((CID_Tab[0] & 0x00FF0000) >> 16);
	cardinfo->SD_cid.OEM_AppliID = tmp << 8;

	/*!< Byte 2 */
	tmp = (uint8_t)((CID_Tab[0] & 0x000000FF00) >> 8);
	cardinfo->SD_cid.OEM_AppliID |= tmp;

	/*!< Byte 3 */
	tmp = (uint8_t)(CID_Tab[0] & 0x000000FF);
	cardinfo->SD_cid.ProdName1 = tmp << 24;

	/*!< Byte 4 */
	tmp = (uint8_t)((CID_Tab[1] & 0xFF000000) >> 24);
	cardinfo->SD_cid.ProdName1 |= tmp << 16;

	/*!< Byte 5 */
	tmp = (uint8_t)((CID_Tab[1] & 0x00FF0000) >> 16);
	cardinfo->SD_cid.ProdName1 |= tmp << 8;

	/*!< Byte 6 */
	tmp = (uint8_t)((CID_Tab[1] & 0x0000FF00) >> 8);
	cardinfo->SD_cid.ProdName1 |= tmp;

	/*!< Byte 7 */
	tmp = (uint8_t)(CID_Tab[1] & 0x000000FF);
	cardinfo->SD_cid.ProdName2 = tmp;

	/*!< Byte 8 */
	tmp = (uint8_t)((CID_Tab[2] & 0xFF000000) >> 24);
	cardinfo->SD_cid.ProdRev = tmp;

	/*!< Byte 9 */
	tmp = (uint8_t)((CID_Tab[2] & 0x00FF0000) >> 16);
	cardinfo->SD_cid.ProdSN = tmp << 24;

	/*!< Byte 10 */
	tmp = (uint8_t)((CID_Tab[2] & 0x0000FF00) >> 8);
	cardinfo->SD_cid.ProdSN |= tmp << 16;

	/*!< Byte 11 */
	tmp = (uint8_t)(CID_Tab[2] & 0x000000FF);
	cardinfo->SD_cid.ProdSN |= tmp << 8;

	/*!< Byte 12 */
	tmp = (uint8_t)((CID_Tab[3] & 0xFF000000) >> 24);
	cardinfo->SD_cid.ProdSN |= tmp;

	/*!< Byte 13 */
	tmp = (uint8_t)((CID_Tab[3] & 0x00FF0000) >> 16);
	cardinfo->SD_cid.Reserved1 |= (tmp & 0xF0) >> 4;
	cardinfo->SD_cid.ManufactDate = (tmp & 0x0F) << 8;

	/*!< Byte 14 */
	tmp = (uint8_t)((CID_Tab[3] & 0x0000FF00) >> 8);
	cardinfo->SD_cid.ManufactDate |= tmp;

	/*!< Byte 15 */
	tmp = (uint8_t)(CID_Tab[3] & 0x000000FF);
	cardinfo->SD_cid.CID_CRC = (tmp & 0xFE) >> 1;
	cardinfo->SD_cid.Reserved2 = 1;

	return(errorstatus);
}

/**
  * @brief  Enables wide bus opeartion for the requeseted card if supported by 
  *         card.
  * @param  WideMode: Specifies the SD card wide bus mode. 
  *   This parameter can be one of the following values:
  *     @arg SDIO_BusWide_8b: 8-bit data transfer (Only for MMC)
  *     @arg SDIO_BusWide_4b: 4-bit data transfer
  *     @arg SDIO_BusWide_1b: 1-bit data transfer
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_GetCardStatus(SD_CardStatus *cardstatus)
{
	SD_Error errorstatus = SD_OK;
	uint8_t tmp = 0;

	errorstatus = SD_SendSDStatus((uint32_t *)SDSTATUS_Tab);

	if (errorstatus  != SD_OK)
	{
		return(errorstatus);
	}

	/*!< Byte 0 */
	tmp = (uint8_t)((SDSTATUS_Tab[0] & 0xC0) >> 6);
	cardstatus->DAT_BUS_WIDTH = tmp;

	/*!< Byte 0 */
	tmp = (uint8_t)((SDSTATUS_Tab[0] & 0x20) >> 5);
	cardstatus->SECURED_MODE = tmp;

	/*!< Byte 2 */
	tmp = (uint8_t)((SDSTATUS_Tab[2] & 0xFF));
	cardstatus->SD_CARD_TYPE = tmp << 8;

	/*!< Byte 3 */
	tmp = (uint8_t)((SDSTATUS_Tab[3] & 0xFF));
	cardstatus->SD_CARD_TYPE |= tmp;

	/*!< Byte 4 */
	tmp = (uint8_t)(SDSTATUS_Tab[4] & 0xFF);
	cardstatus->SIZE_OF_PROTECTED_AREA = tmp << 24;

	/*!< Byte 5 */
	tmp = (uint8_t)(SDSTATUS_Tab[5] & 0xFF);
	cardstatus->SIZE_OF_PROTECTED_AREA |= tmp << 16;

	/*!< Byte 6 */
	tmp = (uint8_t)(SDSTATUS_Tab[6] & 0xFF);
	cardstatus->SIZE_OF_PROTECTED_AREA |= tmp << 8;

	/*!< Byte 7 */
	tmp = (uint8_t)(SDSTATUS_Tab[7] & 0xFF);
	cardstatus->SIZE_OF_PROTECTED_AREA |= tmp;

	/*!< Byte 8 */
	tmp = (uint8_t)((SDSTATUS_Tab[8] & 0xFF));
	cardstatus->SPEED_CLASS = tmp;

	/*!< Byte 9 */
	tmp = (uint8_t)((SDSTATUS_Tab[9] & 0xFF));
	cardstatus->PERFORMANCE_MOVE = tmp;

	/*!< Byte 10 */
	tmp = (uint8_t)((SDSTATUS_Tab[10] & 0xF0) >> 4);
	cardstatus->AU_SIZE = tmp;

	/*!< Byte 11 */
	tmp = (uint8_t)(SDSTATUS_Tab[11] & 0xFF);
	cardstatus->ERASE_SIZE = tmp << 8;

	/*!< Byte 12 */
	tmp = (uint8_t)(SDSTATUS_Tab[12] & 0xFF);
	cardstatus->ERASE_SIZE |= tmp;

	/*!< Byte 13 */
	tmp = (uint8_t)((SDSTATUS_Tab[13] & 0xFC) >> 2);
	cardstatus->ERASE_TIMEOUT = tmp;

	/*!< Byte 13 */
	tmp = (uint8_t)((SDSTATUS_Tab[13] & 0x3));
	cardstatus->ERASE_OFFSET = tmp;

	return(errorstatus);
}

/**************************************************************************/
/*! 
	@brief  Enables wide bus opeartion for the requeseted card if supported by 
			card.
	@param  WideMode: Specifies the SD card wide bus mode. 
		This parameter can be one of the following values:
		@arg SDIO_BusWide_8b: 8-bit data transfer (Only for MMC)
		@arg SDIO_BusWide_4b: 4-bit data transfer
		@arg SDIO_BusWide_1b: 1-bit data transfer
    @retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_EnableWideBusOperation(uint32_t WideMode)
{
	SD_Error errorstatus = SD_OK;

	/*!< MMC Card doesn't support this feature */
	if (SDIO_MULTIMEDIA_CARD == CardType)
	{
		errorstatus = SD_UNSUPPORTED_FEATURE;
		return(errorstatus);
	}
	else if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
	{
		if (SDIO_BusWide_8b == WideMode)
		{
			errorstatus = SD_UNSUPPORTED_FEATURE;
			return(errorstatus);
		}
		else if (SDIO_BusWide_4b == WideMode)
		{
			errorstatus = SDEnWideBus(ENABLE);

			if (SD_OK == errorstatus)
			{
				/*!< Configure the SDIO peripheral */
				SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
				SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
				SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
				SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
				SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_4b;
				SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
				SDIO_Init(&SDIO_InitStructure);
			}
		}
		else
		{
			errorstatus = SDEnWideBus(DISABLE);

			if (SD_OK == errorstatus)
			{
				/*!< Configure the SDIO peripheral */
				SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
				SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
				SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
				SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
				SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_1b;
				SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
				SDIO_Init(&SDIO_InitStructure);
			}
		}
	}

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Selects od Deselects the corresponding card.
	@param  addr: Address of the Card to be selected.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_SelectDeselect(uint64_t addr)
{
	SD_Error errorstatus = SD_OK;

	/*!< Send CMD7 SDIO_SEL_DESEL_CARD */
	SDIO_CmdInitStructure.SDIO_Argument =  (uint32_t) addr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SEL_DESEL_CARD;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SEL_DESEL_CARD);

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Allows to read one block from a specified address in a card.
	@param  readbuff: pointer to the buffer that will contain the received data
	@param  ReadAddr: Address from where data are to be read.  
	@param  BlockSize: the SD card Data block size.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_ReadBlock(uint8_t *readbuff, uint64_t ReadAddr, uint16_t BlockSize)
{
	SD_Error errorstatus = SD_OK;
#if defined (SD_POLLING_MODE)
	uint32_t count = 0, *tempbuff = (uint32_t *)readbuff;
#endif

	TransferError = SD_OK;
	TransferEnd = 0;
	StopCondition = 0;

	SDIO->DCTRL = 0x0;

#if defined(SD_DMA_MODE)
	/* Ready to DMA Before Any SDIO Commands! */
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_RXOVERR | SDIO_IT_STBITERR, ENABLE);
	SD_LowLevel_DMA_RxConfig((uint32_t *)readbuff, BlockSize);
	SDIO_DMACmd(ENABLE);
#endif

	if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		BlockSize = 512;
		ReadAddr /= 512;
	}

	/* Set Block Size for Card */ 
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) BlockSize;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = BlockSize;
	SDIO_DataInitStructure.SDIO_DataBlockSize = (uint32_t) 9 << 4;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

	/*!< Send CMD17 READ_SINGLE_BLOCK */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)ReadAddr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_READ_SINGLE_BLOCK;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_READ_SINGLE_BLOCK);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

#if defined (SD_POLLING_MODE)  
	/*!< In case of single block transfer, no need of stop transfer at all.*/
	/*!< Polling mode */
	while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_RXFIFOHF) != RESET)
		{
			for (count = 0; count < 8; count++)
			{
				*(tempbuff + count) = SDIO_ReadData();
			}
			tempbuff += 8;
		}
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
		errorstatus = SD_RX_OVERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
		return(errorstatus);
	}
	count = SD_DATATIMEOUT;
	while ((SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET) && (count > 0))
	{
		*tempbuff = SDIO_ReadData();
		tempbuff++;
		count--;
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

#elif defined(SD_DMA_MODE)
	/* Check if the Transfer is finished */
	Status = SD_WaitReadOperation();

	/* Wait until end of DMA transfer */
	while(SD_GetStatus() != SD_TRANSFER_OK);
	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
#endif

	return(errorstatus);
}


/**************************************************************************/
/*!
	@brief  Allows to read blocks from a specified address  in a card.
	@param  readbuff: pointer to the buffer that will contain the received data.
	@param  ReadAddr: Address from where data are to be read.
	@param  BlockSize: the SD card Data block size.
	@param  NumberOfBlocks: number of blocks to be read.
	@retval SD_Error: SD Card Error code.
*/

/***************************************************************************/

SD_Error SD_ReadMultiBlocks(uint8_t *readbuff, uint64_t ReadAddr, uint16_t BlockSize, uint32_t NumberOfBlocks)
{
	SD_Error errorstatus = SD_OK;
	TransferError = SD_OK;
	TransferEnd = 0;
	StopCondition = 1;

#if   defined (SD_POLLING_MODE)
	uint32_t count = 0, *tempbuff = (uint32_t *)readbuff;
#endif

	SDIO->DCTRL = 0x0;

#if defined(SD_DMA_MODE)
	/* Ready to DMA Before Any SDIO Commands! */
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_RXOVERR | SDIO_IT_STBITERR, ENABLE);
	SD_LowLevel_DMA_RxConfig((uint32_t *)readbuff, (NumberOfBlocks * BlockSize));
	SDIO_DMACmd(ENABLE);
#endif

	if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		BlockSize = 512;
		ReadAddr /= 512;
	}
//	else  ReadAddr *= BlockSize;

	/*!< Set Block Size for Card */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) BlockSize;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = NumberOfBlocks * BlockSize;
	SDIO_DataInitStructure.SDIO_DataBlockSize = (uint32_t) 9 << 4;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

	/*!< Send CMD18 READ_MULT_BLOCK with argument data address */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)ReadAddr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_READ_MULT_BLOCK;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_READ_MULT_BLOCK);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

#if defined (SD_POLLING_MODE)
	/* Polling mode */
	while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_RXFIFOHF) != RESET)
		{
			for (count = 0; count < SD_HALFFIFO; count++)
			{
				*(tempbuff + count) = SDIO_ReadData();
			}
			tempbuff += SD_HALFFIFO;
		}
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
		errorstatus = SD_RX_OVERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
		return(errorstatus);
	}
	while (SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET)
	{
		*tempbuff = SDIO_ReadData();
		tempbuff++;
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DATAEND) != RESET)
	{
		/* In Case Of SD-CARD Send Command STOP_TRANSMISSION */
		if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType))
		{
			/* Send CMD12 STOP_TRANSMISSION */
			SDIO_CmdInitStructure.SDIO_Argument = 0x0;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_STOP_TRANSMISSION ;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);

			errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);

			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}
		}
	}
	/* Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

#elif defined(SD_DMA_MODE)
	/* Check if the Transfer is finished */
	Status = SD_WaitReadOperation();

	/* Wait until end of DMA transfer */
	while(SD_GetStatus() != SD_TRANSFER_OK);
	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
#endif

	return(errorstatus);
}


SD_Error SD_ReadMultiBlocks2(uint8_t *readbuff, uint64_t ReadAddr, uint16_t BlockSize, uint32_t NumberOfBlocks)
{
	SD_Error errorstatus = SD_OK;
	TransferError = SD_OK;
	TransferEnd = 0;
	StopCondition = 1;

#if   defined (SD_POLLING_MODE)
	uint32_t count = 0, *tempbuff = (uint32_t *)readbuff;
#endif

	SDIO->DCTRL = 0x0;

#if defined(SD_DMA_MODE)
	/* Ready to DMA Before Any SDIO Commands! */
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_RXOVERR | SDIO_IT_STBITERR, ENABLE);
	SD_LowLevel_DMA_RxConfig((uint32_t *)readbuff, (NumberOfBlocks * BlockSize));
	SDIO_DMACmd(ENABLE);
#endif

	if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		BlockSize = 512;
//		ReadAddr /= 512;
	}
	else  ReadAddr *= BlockSize;

	/*!< Set Block Size for Card */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) BlockSize;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = NumberOfBlocks * BlockSize;
	SDIO_DataInitStructure.SDIO_DataBlockSize = (uint32_t) 9 << 4;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

	/*!< Send CMD18 READ_MULT_BLOCK with argument data address */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)ReadAddr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_READ_MULT_BLOCK;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_READ_MULT_BLOCK);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

#if defined (SD_POLLING_MODE)  
	/* Polling mode */
	while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_RXFIFOHF) != RESET)
		{
			for (count = 0; count < SD_HALFFIFO; count++)
			{
				*(tempbuff + count) = SDIO_ReadData();
			}
			tempbuff += SD_HALFFIFO;
		}
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
		errorstatus = SD_RX_OVERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
		return(errorstatus);
	}
	while (SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET)
	{
		*tempbuff = SDIO_ReadData();
		tempbuff++;
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DATAEND) != RESET)
	{
		/* In Case Of SD-CARD Send Command STOP_TRANSMISSION */
		if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType))
		{
			/* Send CMD12 STOP_TRANSMISSION */
			SDIO_CmdInitStructure.SDIO_Argument = 0x0;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_STOP_TRANSMISSION ;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);

			errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);

			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}
		}
	}
	/* Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

#elif defined(SD_DMA_MODE)
	/* Check if the Transfer is finished */
	Status = SD_WaitReadOperation();

	/* Wait until end of DMA transfer */
	while(SD_GetStatus() != SD_TRANSFER_OK);
	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
#endif

	return(errorstatus);
}


/**
  * @brief  This function waits until the SDIO DMA data transfer is finished. 
  *         This function should be called after SDIO_ReadMultiBlocks() function
  *         to insure that all data sent by the card are already transferred by 
  *         the DMA controller.        
  * @param  None.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_WaitReadOperation(void)
{
	SD_Error errorstatus = SD_OK;
	uint32_t timeout;

	timeout = SD_DATATIMEOUT;

	while ((DMAEndOfTransfer == 0x00) && (TransferEnd == 0) && (TransferError == SD_OK) && (timeout > 0))
	{
		timeout--;
	}

	DMAEndOfTransfer = 0x00;

	timeout = SD_DATATIMEOUT;

	while(((SDIO->STA & SDIO_FLAG_RXACT)) && (timeout > 0))
	{
		timeout--;  
	}

	if (StopCondition == 1)
	{
		errorstatus = SD_StopTransfer();
		StopCondition = 0;
	}

	if ((timeout == 0) && (errorstatus == SD_OK))
	{
		errorstatus = SD_DATA_TIMEOUT;
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
	else
	{
		return(errorstatus);  
	}
}


/**************************************************************************/
/*! 
	@brief  Allows to write one block starting from a specified address in a card.
	@param  writebuff: pointer to the buffer that contain the data to be transferred.
	@param  WriteAddr: Address from where data are to be read.   
	@param  BlockSize: the SD card Data block size.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_WriteBlock(uint8_t *writebuff, uint64_t WriteAddr, uint16_t BlockSize)
{
	SD_Error errorstatus = SD_OK;

#if defined (SD_POLLING_MODE)
	SDCardState cardstate =  SD_CARD_TRANSFER;
	uint32_t bytestransferred = 0, count = 0, restwords = 0;
	uint32_t *tempbuff = (uint32_t *)writebuff;
	volatile uint32_t delay;
#endif

	TransferError = SD_OK;
	TransferEnd = 0;
	StopCondition = 0;
	TotalNumberOfBytes = 0;

	SDIO->DCTRL = 0x0;

#if defined(SD_DMA_MODE)
	/* Ready to DMA Before Any SDIO Commands! */
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_TXUNDERR | SDIO_IT_STBITERR, ENABLE);
	SD_LowLevel_DMA_TxConfig((uint32_t *)writebuff, BlockSize);
	SDIO_DMACmd(ENABLE);
#endif

	if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		BlockSize = 512;
		WriteAddr /= 512;
	}

	/* Set Block Size for Card */ 
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) BlockSize;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	/*!< Send CMD24 WRITE_SINGLE_BLOCK */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) WriteAddr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_WRITE_SINGLE_BLOCK;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_WRITE_SINGLE_BLOCK);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = BlockSize;
	SDIO_DataInitStructure.SDIO_DataBlockSize = (uint32_t) 9 << 4;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToCard;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

	/*!< In case of single data block transfer no need of stop command at all */
#if defined (SD_POLLING_MODE) 
	while (!(SDIO->STA & (SDIO_FLAG_DBCKEND | SDIO_FLAG_TXUNDERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_TXFIFOHE) != RESET)
		{
			if ((512 - bytestransferred) < 32)
			{
				restwords = ((512 - bytestransferred) % 4 == 0) ? ((512 - bytestransferred) / 4) : (( 512 -  bytestransferred) / 4 + 1);
				for (count = 0; count < restwords; count++, tempbuff++, bytestransferred += 4)
				{
					SDIO_WriteData(*tempbuff);
				}
			}
			else
			{
				for (count = 0; count < 8; count++)
				{
					SDIO_WriteData(*(tempbuff + count));
				}
				tempbuff += 8;
				bytestransferred += 32;
			}
		}
	}
	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_TXUNDERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_TXUNDERR);
		errorstatus = SD_TX_UNDERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
		return(errorstatus);
	}
  
 	/*!< Wait till the card is in programming state */
	errorstatus = IsCardProgramming(&cardstate);
	delay = SD_DATATIMEOUT;
	while ((delay > 0) && (errorstatus == SD_OK) && ((SD_CARD_PROGRAMMING == cardstate) || (SD_CARD_RECEIVING == cardstate)))
	{
		errorstatus = IsCardProgramming(&cardstate);
		delay--;
	}

	/* Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

#elif defined(SD_DMA_MODE)
	/* Check if the Transfer is finished */
	Status = SD_WaitWriteOperation();

	/* Wait until end of DMA transfer */
	while(SD_GetStatus() != SD_TRANSFER_OK);
	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
#endif

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Allows to write blocks starting from a specified address in a card.
	@param  WriteAddr: Address from where data are to be read.
	@param  writebuff: pointer to the buffer that contain the data to be transferred.
	@param  BlockSize: the SD card Data block size.
	@param  NumberOfBlocks: number of blocks to be written.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_WriteMultiBlocks2(uint8_t *writebuff, uint64_t WriteAddr, uint16_t BlockSize, uint32_t NumberOfBlocks)
{
	SD_Error errorstatus = SD_OK;
#if defined (SD_POLLING_MODE)
	SDCardState cardstate =  SD_CARD_TRANSFER;
	uint32_t bytestransferred = 0, count = 0, restwords = 0;
	uint32_t *tempbuff = (uint32_t *)writebuff;
	volatile uint32_t delay;
#endif

	TransferError = SD_OK;
	TransferEnd = 0;
	StopCondition = 1;
	TotalNumberOfBytes = 0;

	SDIO->DCTRL = 0x0;

#if defined(SD_DMA_MODE)
	/* Ready to DMA Before Any SDIO Commands! */
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_TXUNDERR | SDIO_IT_STBITERR, ENABLE);
	SD_LowLevel_DMA_TxConfig((uint32_t *)writebuff, (NumberOfBlocks * BlockSize));
	SDIO_DMACmd(ENABLE);
#endif

	if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		BlockSize = 512;
//		WriteAddr /= 512;
	}
	else WriteAddr*=BlockSize;

	/* Set Block Size for Card */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) BlockSize;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	/*!< To improve performance */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) (RCA << 16);
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);


	errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}
	/*!< To improve performance */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)NumberOfBlocks;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCK_COUNT;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}


	/*!< Send CMD25 WRITE_MULT_BLOCK with argument data address */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)WriteAddr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_WRITE_MULT_BLOCK;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = NumberOfBlocks * BlockSize;
	SDIO_DataInitStructure.SDIO_DataBlockSize = (uint32_t) 9 << 4;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToCard;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

#if defined (SD_POLLING_MODE)
	TotalNumberOfBytes = NumberOfBlocks * BlockSize;
	while (!(SDIO->STA & (SDIO_FLAG_TXUNDERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_TXFIFOHE) != RESET)
		{
			if (!((TotalNumberOfBytes - bytestransferred) < SD_HALFFIFOBYTES))
			{
				for (count = 0; count < SD_HALFFIFO; count++)
				{
					SDIO_WriteData(*(tempbuff + count));
				}
				tempbuff += SD_HALFFIFO;
				bytestransferred += SD_HALFFIFOBYTES;
			}
			else
			{
				restwords = ((TotalNumberOfBytes - bytestransferred) % 4 == 0) ?
				            ((TotalNumberOfBytes - bytestransferred) / 4) :
							((TotalNumberOfBytes - bytestransferred) / 4 + 1);

				for (count = 0; count < restwords; count++, tempbuff++, bytestransferred += 4)
				{
					SDIO_WriteData(*tempbuff);
				}
			}
		}
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_TXUNDERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_TXUNDERR);
		errorstatus = SD_TX_UNDERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
		return(errorstatus);
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DATAEND) != RESET)
	{
		if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
		{
			/* Send CMD12 STOP_TRANSMISSION */
			SDIO_CmdInitStructure.SDIO_Argument = 0x0;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_STOP_TRANSMISSION;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);


			errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);

			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}
		}
	}

 	/*!< Wait till the card is in programming state */
	errorstatus = IsCardProgramming(&cardstate);
	delay = SD_DATATIMEOUT;
	while ((delay > 0) && (errorstatus == SD_OK) && ((SD_CARD_PROGRAMMING == cardstate) || (SD_CARD_RECEIVING == cardstate)))
	{
		errorstatus = IsCardProgramming(&cardstate);
		delay--;
	}

	/* Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

#elif defined(SD_DMA_MODE)
	/* Check if the Transfer is finished */
	Status = SD_WaitWriteOperation();

	/* Wait until end of DMA transfer */
	while(SD_GetStatus() != SD_TRANSFER_OK);
	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
#endif

	return(errorstatus);
}








SD_Error SD_WriteMultiBlocks(uint8_t *writebuff, uint64_t WriteAddr, uint16_t BlockSize, uint32_t NumberOfBlocks)
{
	SD_Error errorstatus = SD_OK;
#if defined (SD_POLLING_MODE)
	SDCardState cardstate =  SD_CARD_TRANSFER;
	uint32_t bytestransferred = 0, count = 0, restwords = 0;
	uint32_t *tempbuff = (uint32_t *)writebuff;
	volatile uint32_t delay;
#endif

	TransferError = SD_OK;
	TransferEnd = 0;
	StopCondition = 1;
	TotalNumberOfBytes = 0;

	SDIO->DCTRL = 0x0;

#if defined(SD_DMA_MODE)
	/* Ready to DMA Before Any SDIO Commands! */
	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND | SDIO_IT_TXUNDERR | SDIO_IT_STBITERR, ENABLE);
	SD_LowLevel_DMA_TxConfig((uint32_t *)writebuff, (NumberOfBlocks * BlockSize));
	SDIO_DMACmd(ENABLE);
#endif

	if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		BlockSize = 512;
		WriteAddr /= 512;
	}

	/* Set Block Size for Card */ 
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) BlockSize;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	/*!< To improve performance */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) (RCA << 16);
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);


	errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}
	/*!< To improve performance */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)NumberOfBlocks;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCK_COUNT;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}


	/*!< Send CMD25 WRITE_MULT_BLOCK with argument data address */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)WriteAddr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_WRITE_MULT_BLOCK;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);

	if (SD_OK != errorstatus)
	{
		return(errorstatus);
	}

	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = NumberOfBlocks * BlockSize;
	SDIO_DataInitStructure.SDIO_DataBlockSize = (uint32_t) 9 << 4;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToCard;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

#if defined (SD_POLLING_MODE)
	TotalNumberOfBytes = NumberOfBlocks * BlockSize;
	while (!(SDIO->STA & (SDIO_FLAG_TXUNDERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DATAEND | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_TXFIFOHE) != RESET)
		{
			if (!((TotalNumberOfBytes - bytestransferred) < SD_HALFFIFOBYTES))
			{
				for (count = 0; count < SD_HALFFIFO; count++)
				{
					SDIO_WriteData(*(tempbuff + count));
				}
				tempbuff += SD_HALFFIFO;
				bytestransferred += SD_HALFFIFOBYTES;
			}
			else
			{
				restwords = ((TotalNumberOfBytes - bytestransferred) % 4 == 0) ? 
				            ((TotalNumberOfBytes - bytestransferred) / 4) :
							((TotalNumberOfBytes - bytestransferred) / 4 + 1);

				for (count = 0; count < restwords; count++, tempbuff++, bytestransferred += 4)
				{
					SDIO_WriteData(*tempbuff);
				}
			}
		}
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_TXUNDERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_TXUNDERR);
		errorstatus = SD_TX_UNDERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
		return(errorstatus);
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DATAEND) != RESET)
	{
		if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
		{
			/* Send CMD12 STOP_TRANSMISSION */
			SDIO_CmdInitStructure.SDIO_Argument = 0x0;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_STOP_TRANSMISSION;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);


			errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);

			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}
		}
	}
  
 	/*!< Wait till the card is in programming state */
	errorstatus = IsCardProgramming(&cardstate);
	delay = SD_DATATIMEOUT;
	while ((delay > 0) && (errorstatus == SD_OK) && ((SD_CARD_PROGRAMMING == cardstate) || (SD_CARD_RECEIVING == cardstate)))
	{
		errorstatus = IsCardProgramming(&cardstate);
		delay--;
	}

	/* Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

#elif defined(SD_DMA_MODE)
	/* Check if the Transfer is finished */
	Status = SD_WaitWriteOperation();

	/* Wait until end of DMA transfer */
	while(SD_GetStatus() != SD_TRANSFER_OK);
	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
#endif

	return(errorstatus);
}





/**
  * @brief  This function waits until the SDIO DMA data transfer is finished. 
  *         This function should be called after SDIO_WriteBlock() and
  *         SDIO_WriteMultiBlocks() function to insure that all data sent by the 
  *         card are already transferred by the DMA controller.        
  * @param  None.
  * @retval SD_Error: SD Card Error code.
  */
SD_Error SD_WaitWriteOperation(void)
{
	SD_Error errorstatus = SD_OK;
	uint32_t timeout;

	timeout = SD_DATATIMEOUT;

	while ((DMAEndOfTransfer == 0x00) && (TransferEnd == 0) && (TransferError == SD_OK) && (timeout > 0))
	{
		timeout--;
	}

	DMAEndOfTransfer = 0x00;

	timeout = SD_DATATIMEOUT;

	while(((SDIO->STA & SDIO_FLAG_TXACT)) && (timeout > 0))
	{
		timeout--;  
	}

	if (StopCondition == 1)
	{
		errorstatus = SD_StopTransfer();
		StopCondition = 0;
	}

	if ((timeout == 0) && (errorstatus == SD_OK))
	{
		errorstatus = SD_DATA_TIMEOUT;
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	if (TransferError != SD_OK)
	{
		return(TransferError);
	}
	else
	{
		return(errorstatus);
	}
}


/**************************************************************************/
/*! 
	@brief  Gets the cuurent data transfer state.
	@param  None
	@retval SDTransferState: Data Transfer state.
			This value can be: 
			- SD_TRANSFER_OK: No data transfer is acting
			- SD_TRANSFER_BUSY: Data transfer is acting
*/
/**************************************************************************/
SDTransferState SD_GetTransferState(void)
{
	if (SDIO->STA & (SDIO_FLAG_TXACT | SDIO_FLAG_RXACT))
	{
		return(SD_TRANSFER_BUSY);
	}
	else
	{
		return(SD_TRANSFER_OK);
	}
}


/**************************************************************************/
/*! 
	@brief  Aborts an ongoing data transfer.
	@param  None
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_StopTransfer(void)
{
	SD_Error errorstatus = SD_OK;

	/*!< Send CMD12 STOP_TRANSMISSION  */
	SDIO_CmdInitStructure.SDIO_Argument = 0x0;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_STOP_TRANSMISSION;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Allows to erase memory area specified for the given card.
	@param  startaddr: the start address.
	@param  endaddr: the end address.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_Erase(uint64_t startaddr, uint64_t endaddr)
{
	SD_Error errorstatus = SD_OK;
	uint32_t delay = 0;
	__IO uint32_t maxdelay = 0;
	uint8_t cardstate = 0;

	/*!< Check if the card coomnd class supports erase command */
	if (((CSD_Tab[1] >> 20) & SD_CCCC_ERASE) == 0)
	{
		errorstatus = SD_REQUEST_NOT_APPLICABLE;
		return(errorstatus);
	}

	maxdelay = 120000 / ((SDIO->CLKCR & 0xFF) + 2);

	if (SDIO_GetResponse(SDIO_RESP1) & SD_CARD_LOCKED)
	{
		errorstatus = SD_LOCK_UNLOCK_FAILED;
		return(errorstatus);
	}

	if (CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		startaddr /= 512;
		endaddr /= 512;
	}

	/*!< According to sd-card spec 1.0 ERASE_GROUP_START (CMD32) and erase_group_end(CMD33) */
	if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
	{
		/*!< Send CMD32 SD_ERASE_GRP_START with argument as addr  */
		SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) startaddr;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SD_ERASE_GRP_START;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);

		errorstatus = CmdResp1Error(SD_CMD_SD_ERASE_GRP_START);
		if (errorstatus != SD_OK)
		{
			return(errorstatus);
		}

		/*!< Send CMD33 SD_ERASE_GRP_END with argument as addr  */
		SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) endaddr;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SD_ERASE_GRP_END;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);

		errorstatus = CmdResp1Error(SD_CMD_SD_ERASE_GRP_END);
		if (errorstatus != SD_OK)
		{
			return(errorstatus);
		}
	}

	/*!< Send CMD38 ERASE */
	SDIO_CmdInitStructure.SDIO_Argument = 0;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_ERASE;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_ERASE);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	for (delay = 0; delay < maxdelay; delay++){}

	/*!< Wait till the card is in programming state */
	errorstatus = IsCardProgramming(&cardstate);
	delay = SD_DATATIMEOUT;
	while ((delay > 0) && (errorstatus == SD_OK) && ((SD_CARD_PROGRAMMING == cardstate) || (SD_CARD_RECEIVING == cardstate)))
	{
		errorstatus = IsCardProgramming(&cardstate);
		delay--;
	}

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Returns the current card's status.
	@param  pcardstatus: pointer to the buffer that will contain the SD card 
			status (Card Status register).
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_SendStatus(uint32_t *pcardstatus)
{
	SD_Error errorstatus = SD_OK;

	if (pcardstatus == SDIO_NULL)
	{
		errorstatus = SD_INVALID_PARAMETER;
		return(errorstatus);
	}

	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) RCA << 16;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SEND_STATUS;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SEND_STATUS);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	*pcardstatus = SDIO_GetResponse(SDIO_RESP1);

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Returns the current SD card's status.
	@param  psdstatus: pointer to the buffer that will contain the SD card status 
			(SD Status register).
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
SD_Error SD_SendSDStatus(uint32_t *psdstatus)
{
	SD_Error errorstatus = SD_OK;
	uint32_t count = 0;

	if (SDIO_GetResponse(SDIO_RESP1) & SD_CARD_LOCKED)
	{
		errorstatus = SD_LOCK_UNLOCK_FAILED;
		return(errorstatus);
	}

	/*!< Set block size for card if it is not equal to current block size for card. */
	SDIO_CmdInitStructure.SDIO_Argument = 64;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	/*!< CMD55 */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) RCA << 16;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);
	errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = 64;
	SDIO_DataInitStructure.SDIO_DataBlockSize = SDIO_DataBlockSize_64b;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

	/*!< Send ACMD13 SD_APP_STAUS  with argument as card's RCA.*/
	SDIO_CmdInitStructure.SDIO_Argument = 0;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SD_APP_STAUS;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);
	errorstatus = CmdResp1Error(SD_CMD_SD_APP_STAUS);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_RXFIFOHF) != RESET)
		{
			for (count = 0; count < 8; count++)
			{
				*(psdstatus + count) = SDIO_ReadData();
			}
			psdstatus += 8;
		}
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
		errorstatus = SD_RX_OVERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
	return(errorstatus);
	}

	count = SD_DATATIMEOUT;
	while ((SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET) && (count > 0))
	{
		*psdstatus = SDIO_ReadData();
		psdstatus++;
		count--;
	}
	/*!< Clear all the static status flags*/
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Allows to process all the interrupts that are high.
	@param  None
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
inline SD_Error SD_ProcessIRQSrc(void)
{
	if (SDIO_GetITStatus(SDIO_IT_DATAEND) != RESET)
	{
		TransferError = SD_OK;
		SDIO_ClearITPendingBit(SDIO_IT_DATAEND);
		TransferEnd = 1;
	}  
	else if (SDIO_GetITStatus(SDIO_IT_DCRCFAIL) != RESET)
	{
		SDIO_ClearITPendingBit(SDIO_IT_DCRCFAIL);
		TransferError = SD_DATA_CRC_FAIL;
	}
	else if (SDIO_GetITStatus(SDIO_IT_DTIMEOUT) != RESET)
	{
		SDIO_ClearITPendingBit(SDIO_IT_DTIMEOUT);
		TransferError = SD_DATA_TIMEOUT;
	}
	else if (SDIO_GetITStatus(SDIO_IT_RXOVERR) != RESET)
	{
		SDIO_ClearITPendingBit(SDIO_IT_RXOVERR);
		TransferError = SD_RX_OVERRUN;
	}
	else if (SDIO_GetITStatus(SDIO_IT_TXUNDERR) != RESET)
	{
		SDIO_ClearITPendingBit(SDIO_IT_TXUNDERR);
		TransferError = SD_TX_UNDERRUN;
	}
	else if (SDIO_GetITStatus(SDIO_IT_STBITERR) != RESET)
	{
		SDIO_ClearITPendingBit(SDIO_IT_STBITERR);
		TransferError = SD_START_BIT_ERR;
	}

	SDIO_ITConfig(SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | SDIO_IT_DATAEND |
				  SDIO_IT_TXFIFOHE | SDIO_IT_RXFIFOHF | SDIO_IT_TXUNDERR |
				  SDIO_IT_RXOVERR  | SDIO_IT_STBITERR, DISABLE);
	return(TransferError);
}

/**************************************************************************/
/*! 
	@brief  Checks for error conditions for CMD0.
	@param  None
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error CmdError(void)
{
	SD_Error errorstatus = SD_OK;
	uint32_t timeout;

	timeout = SDIO_CMD0TIMEOUT; /*!< 10000 */

	while ((timeout > 0) && (SDIO_GetFlagStatus(SDIO_FLAG_CMDSENT) == RESET))
	{
		timeout--;
	}

	if (timeout == 0)
	{
		errorstatus = SD_CMD_RSP_TIMEOUT;
		return(errorstatus);
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Checks for error conditions for R7 response.
	@param  None
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error CmdResp7Error(void)
{
	SD_Error errorstatus = SD_OK;
	uint32_t status;
	uint32_t timeout = SDIO_CMD0TIMEOUT;

	status = SDIO->STA;

	while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)) && (timeout > 0))
	{
		timeout--;
		status = SDIO->STA;
	}

	if ((timeout == 0) || (status & SDIO_FLAG_CTIMEOUT))
	{
		/*!< Card is not V2.0 complient or card does not support the set voltage range */
		errorstatus = SD_CMD_RSP_TIMEOUT;
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return(errorstatus);
	}

	if (status & SDIO_FLAG_CMDREND)
	{
		/*!< Card is SD V2.0 compliant */
		errorstatus = SD_OK;
		SDIO_ClearFlag(SDIO_FLAG_CMDREND);
		return(errorstatus);
	}
	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Checks for error conditions for R1 response.
	@param  cmd: The sent command index.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error CmdResp1Error(uint8_t cmd)
{
	SD_Error errorstatus = SD_OK;
	uint32_t status;
	uint32_t response_r1;

	status = SDIO->STA;

	while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)))
	{
		status = SDIO->STA;
	}

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		errorstatus = SD_CMD_RSP_TIMEOUT;
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return(errorstatus);
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		errorstatus = SD_CMD_CRC_FAIL;
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return(errorstatus);
	}

	/*!< Check response received is of desired command */
	if (SDIO_GetCommandResponse() != cmd)
	{
		errorstatus = SD_ILLEGAL_CMD;
		return(errorstatus);
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	/*!< We have received response, retrieve it for analysis  */
	response_r1 = SDIO_GetResponse(SDIO_RESP1);

	if ((response_r1 & SD_OCR_ERRORBITS) == SD_ALLZERO)
	{
		return(errorstatus);
	}

	if (response_r1 & SD_OCR_ADDR_OUT_OF_RANGE)
	{
		return(SD_ADDR_OUT_OF_RANGE);
	}

	if (response_r1 & SD_OCR_ADDR_MISALIGNED)
	{
		return(SD_ADDR_MISALIGNED);
	}

	if (response_r1 & SD_OCR_BLOCK_LEN_ERR)
	{
		return(SD_BLOCK_LEN_ERR);
	}

	if (response_r1 & SD_OCR_ERASE_SEQ_ERR)
	{
		return(SD_ERASE_SEQ_ERR);
	}

	if (response_r1 & SD_OCR_BAD_ERASE_PARAM)
	{
		return(SD_BAD_ERASE_PARAM);
	}

	if (response_r1 & SD_OCR_WRITE_PROT_VIOLATION)
	{
		return(SD_WRITE_PROT_VIOLATION);
	}

	if (response_r1 & SD_OCR_LOCK_UNLOCK_FAILED)
	{
		return(SD_LOCK_UNLOCK_FAILED);
	}

	if (response_r1 & SD_OCR_COM_CRC_FAILED)
	{
		return(SD_COM_CRC_FAILED);
	}

	if (response_r1 & SD_OCR_ILLEGAL_CMD)
	{
		return(SD_ILLEGAL_CMD);
	}

	if (response_r1 & SD_OCR_CARD_ECC_FAILED)
	{
		return(SD_CARD_ECC_FAILED);
	}

	if (response_r1 & SD_OCR_CC_ERROR)
	{
		return(SD_CC_ERROR);
	}

	if (response_r1 & SD_OCR_GENERAL_UNKNOWN_ERROR)
	{
		return(SD_GENERAL_UNKNOWN_ERROR);
	}

	if (response_r1 & SD_OCR_STREAM_READ_UNDERRUN)
	{
		return(SD_STREAM_READ_UNDERRUN);
	}

	if (response_r1 & SD_OCR_STREAM_WRITE_OVERRUN)
	{
		return(SD_STREAM_WRITE_OVERRUN);
	}

	if (response_r1 & SD_OCR_CID_CSD_OVERWRIETE)
	{
		return(SD_CID_CSD_OVERWRITE);
	}

	if (response_r1 & SD_OCR_WP_ERASE_SKIP)
	{
		return(SD_WP_ERASE_SKIP);
	}

	if (response_r1 & SD_OCR_CARD_ECC_DISABLED)
	{
		return(SD_CARD_ECC_DISABLED);
	}

	if (response_r1 & SD_OCR_ERASE_RESET)
	{
		return(SD_ERASE_RESET);
	}

	if (response_r1 & SD_OCR_AKE_SEQ_ERROR)
	{
		return(SD_AKE_SEQ_ERROR);
	}

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Checks for error conditions for R3 (OCR) response.
	@param  None
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error CmdResp3Error(void)
{
	SD_Error errorstatus = SD_OK;
	uint32_t status;

	status = SDIO->STA;

	while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)))
	{
		status = SDIO->STA;
	}

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		errorstatus = SD_CMD_RSP_TIMEOUT;
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return(errorstatus);
	}
	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);
	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Checks for error conditions for R2 (CID or CSD) response.
	@param  None
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error CmdResp2Error(void)
{
	SD_Error errorstatus = SD_OK;
	uint32_t status;

	status = SDIO->STA;

	while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CMDREND)))
	{
		status = SDIO->STA;
	}

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		errorstatus = SD_CMD_RSP_TIMEOUT;
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return(errorstatus);
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		errorstatus = SD_CMD_CRC_FAIL;
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return(errorstatus);
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Checks for error conditions for R6 (RCA) response.
	@param  cmd: The sent command index.
	@param  prca: pointer to the variable that will contain the SD card relative 
			address RCA. 
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error CmdResp6Error(uint8_t cmd, uint16_t *prca)
{
	SD_Error errorstatus = SD_OK;
	uint32_t status;
	uint32_t response_r1;

	status = SDIO->STA;

	while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CMDREND)))
	{
		status = SDIO->STA;
	}

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		errorstatus = SD_CMD_RSP_TIMEOUT;
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return(errorstatus);
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		errorstatus = SD_CMD_CRC_FAIL;
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return(errorstatus);
	}

	/*!< Check response received is of desired command */
	if (SDIO_GetCommandResponse() != cmd)
	{
		errorstatus = SD_ILLEGAL_CMD;
		return(errorstatus);
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	/*!< We have received response, retrieve it.  */
	response_r1 = SDIO_GetResponse(SDIO_RESP1);

	if (SD_ALLZERO == (response_r1 & (SD_R6_GENERAL_UNKNOWN_ERROR | SD_R6_ILLEGAL_CMD | SD_R6_COM_CRC_FAILED)))
	{
		*prca = (uint16_t) (response_r1 >> 16);
		return(errorstatus);
	}

	if (response_r1 & SD_R6_GENERAL_UNKNOWN_ERROR)
	{
		return(SD_GENERAL_UNKNOWN_ERROR);
	}

	if (response_r1 & SD_R6_ILLEGAL_CMD)
	{
		return(SD_ILLEGAL_CMD);
	}

	if (response_r1 & SD_R6_COM_CRC_FAILED)
	{
		return(SD_COM_CRC_FAILED);
	}

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Enables or disables the SDIO wide bus mode.
	@param  NewState: new state of the SDIO wide bus mode.
		This parameter can be: ENABLE or DISABLE.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error SDEnWideBus(FunctionalState NewState)
{
	SD_Error errorstatus = SD_OK;

	uint32_t scr[2] = {0, 0};

	if (SDIO_GetResponse(SDIO_RESP1) & SD_CARD_LOCKED)
	{
		errorstatus = SD_LOCK_UNLOCK_FAILED;
		return(errorstatus);
	}

	/*!< Get SCR Register */
	errorstatus = FindSCR(RCA, scr);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	/*!< If wide bus operation to be enabled */
	if (NewState == ENABLE)
	{	
		/*!< If requested card supports wide bus operation */
		if ((scr[1] & SD_WIDE_BUS_SUPPORT) != SD_ALLZERO)
		{
			/*!< Send CMD55 APP_CMD with argument as card's RCA.*/
			SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) RCA << 16;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);

			errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

			if (errorstatus != SD_OK)
			{
			return(errorstatus);
			}

			/*!< Send ACMD6 APP_CMD with argument as 2 for wide bus mode */
			SDIO_CmdInitStructure.SDIO_Argument = 0x2;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_SD_SET_BUSWIDTH;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);

			errorstatus = CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);

			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}

			return(errorstatus);
		}
		else
		{
			errorstatus = SD_REQUEST_NOT_APPLICABLE;
			return(errorstatus);
		}
	}   /*!< If wide bus operation to be disabled */
	else
	{
		/*!< If requested card supports 1 bit mode operation */
		if ((scr[1] & SD_SINGLE_BUS_SUPPORT) != SD_ALLZERO)
		{
			/*!< Send CMD55 APP_CMD with argument as card's RCA.*/
			SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) RCA << 16;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);


			errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}

			/*!< Send ACMD6 APP_CMD with argument as 2 for wide bus mode */
			SDIO_CmdInitStructure.SDIO_Argument = 0x00;
			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_SD_SET_BUSWIDTH;
			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
			SDIO_SendCommand(&SDIO_CmdInitStructure);

			errorstatus = CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);

			if (errorstatus != SD_OK)
			{
				return(errorstatus);
			}

			return(errorstatus);
		}
		else
		{
			errorstatus = SD_REQUEST_NOT_APPLICABLE;
			return(errorstatus);
		}
	}
}


/**************************************************************************/
/*! 
	@brief  Checks if the SD card is in programming state.
	@param  pstatus: pointer to the variable that will contain the SD card state.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error IsCardProgramming(uint8_t *pstatus)
{
	SD_Error errorstatus = SD_OK;
	__IO uint32_t respR1 = 0, status = 0;

	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) RCA << 16;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SEND_STATUS;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	status = SDIO->STA;
	while (!(status & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT)))
	{
		status = SDIO->STA;
	}

	if (status & SDIO_FLAG_CTIMEOUT)
	{
		errorstatus = SD_CMD_RSP_TIMEOUT;
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return(errorstatus);
	}
	else if (status & SDIO_FLAG_CCRCFAIL)
	{
		errorstatus = SD_CMD_CRC_FAIL;
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return(errorstatus);
	}

	status = (uint32_t)SDIO_GetCommandResponse();

	/*!< Check response received is of desired command */
	if (status != SD_CMD_SEND_STATUS)
	{
		errorstatus = SD_ILLEGAL_CMD;
		return(errorstatus);
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);


	/*!< We have received response, retrieve it for analysis  */
	respR1 = SDIO_GetResponse(SDIO_RESP1);

	/*!< Find out card status */
	*pstatus = (uint8_t) ((respR1 >> 9) & 0x0000000F);

	if ((respR1 & SD_OCR_ERRORBITS) == SD_ALLZERO)
	{
		return(errorstatus);
	}

	if (respR1 & SD_OCR_ADDR_OUT_OF_RANGE)
	{
		return(SD_ADDR_OUT_OF_RANGE);
	}

	if (respR1 & SD_OCR_ADDR_MISALIGNED)
	{
		return(SD_ADDR_MISALIGNED);
	}

	if (respR1 & SD_OCR_BLOCK_LEN_ERR)
	{
		return(SD_BLOCK_LEN_ERR);
	}

	if (respR1 & SD_OCR_ERASE_SEQ_ERR)
	{
		return(SD_ERASE_SEQ_ERR);
	}

	if (respR1 & SD_OCR_BAD_ERASE_PARAM)
	{
		return(SD_BAD_ERASE_PARAM);
	}

	if (respR1 & SD_OCR_WRITE_PROT_VIOLATION)
	{
		return(SD_WRITE_PROT_VIOLATION);
	}

	if (respR1 & SD_OCR_LOCK_UNLOCK_FAILED)
	{
		return(SD_LOCK_UNLOCK_FAILED);
	}

	if (respR1 & SD_OCR_COM_CRC_FAILED)
	{
		return(SD_COM_CRC_FAILED);
	}

	if (respR1 & SD_OCR_ILLEGAL_CMD)
	{
		return(SD_ILLEGAL_CMD);
	}

	if (respR1 & SD_OCR_CARD_ECC_FAILED)
	{
		return(SD_CARD_ECC_FAILED);
	}

	if (respR1 & SD_OCR_CC_ERROR)
	{
		return(SD_CC_ERROR);
	}

	if (respR1 & SD_OCR_GENERAL_UNKNOWN_ERROR)
	{
		return(SD_GENERAL_UNKNOWN_ERROR);
	}

	if (respR1 & SD_OCR_STREAM_READ_UNDERRUN)
	{
		return(SD_STREAM_READ_UNDERRUN);
	}

	if (respR1 & SD_OCR_STREAM_WRITE_OVERRUN)
	{
		return(SD_STREAM_WRITE_OVERRUN);
	}

	if (respR1 & SD_OCR_CID_CSD_OVERWRIETE)
	{
		return(SD_CID_CSD_OVERWRITE);
	}

	if (respR1 & SD_OCR_WP_ERASE_SKIP)
	{
		return(SD_WP_ERASE_SKIP);
	}

	if (respR1 & SD_OCR_CARD_ECC_DISABLED)
	{
		return(SD_CARD_ECC_DISABLED);
	}

	if (respR1 & SD_OCR_ERASE_RESET)
	{
		return(SD_ERASE_RESET);
	}

	if (respR1 & SD_OCR_AKE_SEQ_ERROR)
	{
		return(SD_AKE_SEQ_ERROR);
	}

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Find the SD card SCR register value.
	@param  rca: selected card address.
	@param  pscr: pointer to the buffer that will contain the SCR value.
	@retval SD_Error: SD Card Error code.
*/
/**************************************************************************/
static SD_Error FindSCR(uint16_t rca, uint32_t *pscr)
{
	uint32_t index = 0;
	SD_Error errorstatus = SD_OK;
	uint32_t tempscr[2] = {0, 0};

	/*!< Set Block Size To 8 Bytes */
	/*!< Send CMD55 APP_CMD with argument as card's RCA */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)8;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	/*!< Send CMD55 APP_CMD with argument as card's RCA */
	SDIO_CmdInitStructure.SDIO_Argument = (uint32_t) RCA << 16;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}
	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = 8;
	SDIO_DataInitStructure.SDIO_DataBlockSize = SDIO_DataBlockSize_8b;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);


	/*!< Send ACMD51 SD_APP_SEND_SCR with argument as 0 */
	SDIO_CmdInitStructure.SDIO_Argument = 0x0;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SD_APP_SEND_SCR;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);

	errorstatus = CmdResp1Error(SD_CMD_SD_APP_SEND_SCR);

	if (errorstatus != SD_OK)
	{
		return(errorstatus);
	}

	while (!(SDIO->STA & (SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
	{
		if (SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET)
		{
			*(tempscr + index) = SDIO_ReadData();
			index++;
		}
	}

	if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
		errorstatus = SD_DATA_TIMEOUT;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
		errorstatus = SD_DATA_CRC_FAIL;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
		errorstatus = SD_RX_OVERRUN;
		return(errorstatus);
	}
	else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
	{
		SDIO_ClearFlag(SDIO_FLAG_STBITERR);
		errorstatus = SD_START_BIT_ERR;
		return(errorstatus);
	}

	/*!< Clear all the static flags */
	SDIO_ClearFlag(SDIO_STATIC_FLAGS);

	*(pscr + 1) = ((tempscr[0] & SD_0TO7BITS) << 24) | ((tempscr[0] & SD_8TO15BITS) << 8) | ((tempscr[0] & SD_16TO23BITS) >> 8) | ((tempscr[0] & SD_24TO31BITS) >> 24);

	*(pscr) = ((tempscr[1] & SD_0TO7BITS) << 24) | ((tempscr[1] & SD_8TO15BITS) << 8) | ((tempscr[1] & SD_16TO23BITS) >> 8) | ((tempscr[1] & SD_24TO31BITS) >> 24);

	return(errorstatus);
}


/**************************************************************************/
/*! 
	@brief  Switch mode High-SpeedMode(48MHz SDIO Clock).			@n
			But may not work properly due to f**kin' errata!		@n
			See sdio_stm32f4.h line 48 for more information.
	@retval None
*/
/**************************************************************************/
SD_Error SD_HighSpeed (void)
{
	SD_Error errorstatus = SD_OK;
	uint32_t scr[2] = {0, 0};
	uint32_t SD_SPEC = 0 ;
	uint8_t hs[64] = {0} ;
	uint32_t  count = 0, *tempbuff = (uint32_t *)hs;
	TransferError = SD_OK;
	TransferEnd = 0;
	StopCondition = 0;

	SDIO->DCTRL = 0x0;

	/*!< Get SCR Register */
	errorstatus = FindSCR(RCA, scr);

	if (errorstatus != SD_OK)
	{
	return(errorstatus);
	}

	/* Test the Version supported by the card*/ 
	SD_SPEC = (scr[1]  & 0x01000000)||(scr[1]  & 0x02000000);

	if (SD_SPEC != SD_ALLZERO)
	{
		/* Set Block Size for Card */
		SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)64;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);
		if (errorstatus != SD_OK)
		{
			return(errorstatus);
		}
		SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
		SDIO_DataInitStructure.SDIO_DataLength = 64;
		SDIO_DataInitStructure.SDIO_DataBlockSize = SDIO_DataBlockSize_64b ;
		SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
		SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
		SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
		SDIO_DataConfig(&SDIO_DataInitStructure);

		/*!< Send CMD6 switch mode */
		SDIO_CmdInitStructure.SDIO_Argument = 0x80FFFF01;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_HS_SWITCH;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure); 
		errorstatus = CmdResp1Error(SD_CMD_HS_SWITCH);

		if (errorstatus != SD_OK)
		{
			return(errorstatus);
		}
		while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
		{
			if (SDIO_GetFlagStatus(SDIO_FLAG_RXFIFOHF) != RESET)
			{
				for (count = 0; count < 8; count++)
				{
					*(tempbuff + count) = SDIO_ReadData();
				}
				tempbuff += 8;
			}	
		}

		if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
			errorstatus = SD_DATA_TIMEOUT;
			return(errorstatus);
		}
		else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
			errorstatus = SD_DATA_CRC_FAIL;
			return(errorstatus);
		}
		else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
			errorstatus = SD_RX_OVERRUN;
			return(errorstatus);
		}
		else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_STBITERR);
			errorstatus = SD_START_BIT_ERR;
			return(errorstatus);
		}
		count = SD_DATATIMEOUT;
		while ((SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET) && (count > 0))
		{
			*tempbuff = SDIO_ReadData();
			tempbuff++;
			count--;
		}

		/*!< Clear all the static flags */
		SDIO_ClearFlag(SDIO_STATIC_FLAGS);

		/* Test if the switch mode HS is ok */
		if ((hs[13]& 0x2)==0x2)
		{
			/*!< Configure the SDIO peripheral */
			SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
		#if defined (STM32F40_41xxx)
			SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Falling;		/* This is a baddest work around for STM32F40x and STM32F41x */
		#else
			SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
		#endif
			SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Enable;	/* Set Direct Clock(48MHz) */
			SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
			SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_4b;
			SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
			SDIO_Init(&SDIO_InitStructure);
			errorstatus=SD_OK;	/* Enter SDHighSpeedMode */
		}
		else
		{
			errorstatus=SD_OK;	/* Still SDNomalMode */
			/*errorstatus=SD_UNSUPPORTED_FEATURE ;*/
		}  
	}
	return(errorstatus);
}

/**************************************************************************/
/*! 
	@brief  Converts the number of bytes in power of two and returns the power.
	@param  NumberOfBytes: number of bytes.
	@retval None
*/
/**************************************************************************/
uint8_t convert_from_bytes_to_power_of_two(uint16_t NumberOfBytes)
{
	uint8_t count = 0;

	while (NumberOfBytes != 1)
	{
		NumberOfBytes >>= 1;
		count++;
	}
	return(count);
}

/**************************************************************************/
/*! 
    @brief	Handles SDIO interrupts requests.
	@param	None.
    @retval	None.
*/
/**************************************************************************/
void SDIO_IRQHandler(void)
{
	/* Process All SDIO Interrupt Sources */
	SD_ProcessIRQSrc();
}

#if defined(SD_DMA_MODE)
/**************************************************************************/
/*! 
	@brief  Configures the DMA2 Stream3 or 6 for SDIO Tx request.
	@param  BufferSRC: pointer to the source buffer
	@param  BufferSize: buffer size
	@retval None
*/
/**************************************************************************/
void SD_LowLevel_DMA_TxConfig(uint32_t *BufferSRC, uint32_t BufferSize)
{
	DMA_ClearFlag(SD_SDIO_DMA_STREAM, SD_SDIO_DMA_FLAG_FEIF | SD_SDIO_DMA_FLAG_DMEIF | SD_SDIO_DMA_FLAG_TEIF | SD_SDIO_DMA_FLAG_HTIF | SD_SDIO_DMA_FLAG_TCIF);

	/* DMA2 Stream3  or Stream6 disable */
	DMA_Cmd(SD_SDIO_DMA_STREAM, DISABLE);

	/* DMA2 Stream3  or Stream6 Config */
	DMA_DeInit(SD_SDIO_DMA_STREAM);

	/* Set DMA Structure */
	SDDMA_InitStructure.DMA_Memory0BaseAddr 	= (uint32_t)BufferSRC;
	SDDMA_InitStructure.DMA_DIR 				= DMA_DIR_MemoryToPeripheral;
	SDDMA_InitStructure.DMA_BufferSize 			= BufferSize;
	DMA_Init(SD_SDIO_DMA_STREAM, &SDDMA_InitStructure);
	DMA_ITConfig(SD_SDIO_DMA_STREAM, DMA_IT_TC, ENABLE);
	DMA_FlowControllerConfig(SD_SDIO_DMA_STREAM, DMA_FlowCtrl_Peripheral);

	/* DMA2 Stream3  or Stream6 enable */
	DMA_Cmd(SD_SDIO_DMA_STREAM, ENABLE);
}


/**************************************************************************/
/*! 
	@brief  Configures the DMA2 Stream3 or 6 for SDIO Rx request.
	@param  BufferDST: pointer to the destination buffer
	@param  BufferSize: buffer size
	@retval None
*/
/**************************************************************************/
void SD_LowLevel_DMA_RxConfig(uint32_t *BufferDST, uint32_t BufferSize)
{
	DMA_ClearFlag(SD_SDIO_DMA_STREAM, SD_SDIO_DMA_FLAG_FEIF | SD_SDIO_DMA_FLAG_DMEIF | SD_SDIO_DMA_FLAG_TEIF | SD_SDIO_DMA_FLAG_HTIF | SD_SDIO_DMA_FLAG_TCIF);

	/* DMA2 Stream3  or Stream6 disable */
	DMA_Cmd(SD_SDIO_DMA_STREAM, DISABLE);

	/* DMA2 Stream3 or Stream6 Config */
	DMA_DeInit(SD_SDIO_DMA_STREAM);

	/* Set DMA Structure */
	SDDMA_InitStructure.DMA_Memory0BaseAddr 	= (uint32_t)BufferDST;
	SDDMA_InitStructure.DMA_DIR 				= DMA_DIR_PeripheralToMemory;
	SDDMA_InitStructure.DMA_BufferSize 			= BufferSize;
	DMA_Init(SD_SDIO_DMA_STREAM, &SDDMA_InitStructure);
	DMA_ITConfig(SD_SDIO_DMA_STREAM, DMA_IT_TC, ENABLE);
	DMA_FlowControllerConfig(SD_SDIO_DMA_STREAM, DMA_FlowCtrl_Peripheral);

	/* DMA2 Stream3 or Stream6 enable */
	DMA_Cmd(SD_SDIO_DMA_STREAM, ENABLE);
}

/**************************************************************************/
/*! 
    @brief	Process DMA2 Stream3 or DMA2 Stream6 Interrupt Sources.
	@param	None.
    @retval	None.
*/
/**************************************************************************/
void SD_SDIO_DMA_IRQHANDLER(void)
{
	/* Process DMA2 Stream3 or DMA2 Stream6 Interrupt Sources */
	if(SD_SDIO_DMA_xISR & SD_SDIO_DMA_FLAG_TCIF)
	{
		DMAEndOfTransfer = 0x01;
		DMA_ClearFlag(SD_SDIO_DMA_STREAM, SD_SDIO_DMA_FLAG_TCIF|SD_SDIO_DMA_FLAG_FEIF);
	}
}
#endif



/**************************************************************************/
/*! 
    Public Functions For FatFs.
*/
/**************************************************************************/
/**************************************************************************/
/*! 
    @brief Initialize a Drive.
	@param  drv     : Physical drive number (0..).
    @retval DSTATUS :
*/
/**************************************************************************/
DSTATUS disk_initialize(uint8_t drv)
{
	switch (drv)
	{
		case SDIO_DRIVE:
		{
			/* Initialize SD Card */
			Status = SD_Init();

			if (Status != SD_OK)
				return STA_NOINIT;
			else
				return 0x00;
		}
  }

  return STA_NOINIT;

}

/**************************************************************************/
/*! 
    @brief Return Disk Status.
	@param  drv     : Physical drive number (0..).
    @retval DSTATUS :
*/
/**************************************************************************/
DSTATUS disk_status(uint8_t drv)
{
	switch (drv)
	{
		case SDIO_DRIVE:
		{
			Status = SD_GetCardInfo(&SDCardInfo);

			if (Status != SD_OK)
				return STA_NOINIT;
			else
				return 0x00;
		}
	}

	return STA_NOINIT;
}

/**************************************************************************/
/*! 
    @brief Read Sector(s).
	@param  drv     : Physical drive number (0..).
	@param  *buff   : Data buffer to store read data.
	@param  sector  : Sector address (LBA).
	@param  count   : Number of sectors to read.
    @retval DSTATUS :
*/
/**************************************************************************/
DRESULT disk_read(uint8_t drv,uint8_t *buff,uint32_t sector,unsigned int count)
{
	switch (drv)
	{
		case SDIO_DRIVE:
		{
			Status = SD_OK;

		#if defined(SD_DMA_MODE) && (NO_ALIGN4CHK == 0)
			if((uint32_t)buff & 3)	/* Check 4-Byte Alignment */
			{	/* Unaligned Buffer Address Case (Slower) */
				unsigned int secNum;
				for (secNum = 0; secNum < count && Status == SD_OK; secNum++){
					Status =  SD_ReadBlock(dmabuf,
								  (uint64_t)(sector+secNum)*SECTOR_SIZE,
								  (uint8_t)SECTOR_SIZE);
					/* Use optimized memcpy for ARMv7-M, std memcpy was override by optimized one. */
					memcpy(buff+SECTOR_SIZE*secNum, dmabuf, SECTOR_SIZE);
				}
			} else {
				/* Aligned Buffer Address Case (Faster) */
				if(count==1){
					Status = SD_ReadBlock((uint8_t*)(buff),
										  ((uint64_t)(sector)*SECTOR_SIZE),
										  SECTOR_SIZE);
				}
				else{
					Status = SD_ReadMultiBlocks((uint8_t*)(buff),
												((uint64_t)(sector)*SECTOR_SIZE),
												SECTOR_SIZE
												,count);
				}
			}

		#else	/* POLLING MODE or NO Aligned Check DMA MODE */
			if(count==1){
				Status = SD_ReadBlock((uint8_t*)(buff),
									  ((uint64_t)(sector)*SECTOR_SIZE),
									  SECTOR_SIZE);
			}
			else{
				Status = SD_ReadMultiBlocks((uint8_t*)(buff),
											((uint64_t)(sector)*SECTOR_SIZE),
											SECTOR_SIZE
											,count);
			}
		#endif

			if (Status == SD_OK)	return RES_OK;
			else					return RES_ERROR;
		}
	}
	return RES_PARERR;
}

/**************************************************************************/
/*!
    @brief Write Sector(s).
	@param  drv     : Physical drive number (0..).
	@param  *buff   : Data to be written.
	@param  sector  : Sector address (LBA).
	@param  count   : Number of sectors to write.
    @retval DSTATUS :
*/
/**************************************************************************/
#if _READONLY == 0
DRESULT disk_write(uint8_t drv,const uint8_t *buff,uint32_t sector,unsigned int count)
{
	switch (drv)
	{
		case SDIO_DRIVE:
		{
			Status = SD_OK;

		#if defined(SD_DMA_MODE) && (NO_ALIGN4CHK == 0)
			if((uint32_t)buff & 3)	/* Check 4-Byte Alignment */
			{	/* Unaligned Buffer Address Case (Slower) */
				unsigned int secNum;
				for (secNum = 0; secNum < count && Status == SD_OK; secNum++){
					/* Use optimized memcpy for ARMv7-M, std memcpy was override by optimized one. */
					memcpy(dmabuf, buff+SECTOR_SIZE*secNum, SECTOR_SIZE);
					Status = SD_WriteBlock(dmabuf,
								  (uint64_t)(sector+secNum)*SECTOR_SIZE,
								  (uint8_t)SECTOR_SIZE);
				}
			} else {
				/* Aligned Buffer Address Case (Faster) */
				if(count==1){
					Status = SD_WriteBlock((uint8_t*)(buff),
										  ((uint64_t)(sector)*SECTOR_SIZE),
										  SECTOR_SIZE);
				}
				else{
					Status = SD_WriteMultiBlocks((uint8_t*)(buff),
												((uint64_t)(sector)*SECTOR_SIZE),
												SECTOR_SIZE
												,count);
				}
			}

		#else	/* POLLING MODE or NO Aligned Check DMA MODE */
		#if defined(SD_DMA_MODE) && (NO_ALIGN4CHK == 1)
		 #warning "You are about to DMA Tx without unaligned check!"
		#endif
			if(count==1){
				Status = SD_WriteBlock((uint8_t*)(buff),
									  ((uint64_t)(sector)*SECTOR_SIZE),
									  SECTOR_SIZE);
			}
			else{
				Status = SD_WriteMultiBlocks((uint8_t*)(buff),
											((uint64_t)(sector)*SECTOR_SIZE),
											SECTOR_SIZE
											,count);
			}
		#endif

			if (Status == SD_OK)	return RES_OK;
			else					return RES_ERROR;
		}
	}
	return RES_PARERR;
}
#endif /* _READONLY */

/**************************************************************************/
/*!
    @brief Miscellaneous Functions.
	@param  drv     : Physical drive number (0..).
	@param  ctrl    : Control code.
	@param  *buff   : Buffer to send/receive control data.
    @retval DSTATUS :
*/
/**************************************************************************/
DRESULT disk_ioctl(uint8_t drv,uint8_t ctrl,void *buff)
{
	switch (drv)
	{
		case SDIO_DRIVE:
		{
		  switch (ctrl)
		  {
			case CTRL_SYNC:
			  /* no synchronization to do since not buffering in this module */
			  return RES_OK;
			case GET_SECTOR_SIZE:
			  *(uint16_t*)buff = SECTOR_SIZE;
			  return RES_OK;
			case GET_SECTOR_COUNT:
			  *(uint32_t*)buff = SDCardInfo.CardCapacity / SECTOR_SIZE;
			  return RES_OK;
			case GET_BLOCK_SIZE:
			  *(uint32_t*)buff = SDCardInfo.CardBlockSize;
			  return RES_OK;

			/* Following command are not used by FatFs module */
			case MMC_GET_TYPE :		/* Get MMC/SDC type (uint8_t) */
				switch (SDCardInfo.CardType)
				{
					case SDIO_STD_CAPACITY_SD_CARD_V1_1:
						*(uint8_t*)buff = CT_SD1;
						break;
					case SDIO_STD_CAPACITY_SD_CARD_V2_0:
						*(uint8_t*)buff = CT_SD2;
						break;
					case SDIO_HIGH_CAPACITY_SD_CARD:
						*(uint8_t*)buff = CT_SD2 | CT_BLOCK;
						break;
					case SDIO_MULTIMEDIA_CARD:
					case SDIO_HIGH_SPEED_MULTIMEDIA_CARD:
					case SDIO_HIGH_CAPACITY_MMC_CARD:
						*(uint8_t*)buff = CT_MMC;
						break;
					default:
						*(uint8_t*)buff = 0;
				}
				return RES_OK;
			case MMC_GET_CSD :		/* Read CSD (16 bytes) */
				memcpy((void *)buff,&SDCardInfo.SD_csd,16);
				return RES_OK;
			case MMC_GET_CID :		/* Read CID (16 bytes) */
				memcpy((void *)buff,&SDCardInfo.SD_cid,16);
				return RES_OK;
			case MMC_GET_OCR :		/* Read OCR (4 bytes) */
				*(uint32_t*)buff = SDCardInfo.SD_csd.MaxRdCurrentVDDMin;
				return RES_OK;
			case MMC_GET_SDSTAT :	/* Read SD status (64 bytes) */
				SD_GetCardStatus(&SDCardStatus);
				memcpy((void *)buff,&SDCardStatus,64);
				return RES_OK;
			default :
				return RES_OK;

			}
		}
	}
	return RES_PARERR;
}


/**************************************************************************/
/*! 
    @brief Device Timer Interrupt Procedure.								@n
		   This function must be called in period of 10ms.
	@param  none
    @retval none
*/
/**************************************************************************/
void disk_timerproc(void)
{
	uint8_t n, s;

	n = Timer1;					/* 100Hz decrement timer */
	if (n) Timer1 = --n;
	n = Timer2;
	if (n) Timer2 = --n;


	s = Stat;
	if (SOCKWP)					/* WP is H (write protected) */
		s |= STA_PROTECT;
	else						/* WP is L (write enabled) */
		s &= ~STA_PROTECT;

	if (!SD_Detect())			/* INS = H (Socket empty) */
		s |= (STA_NODISK | STA_NOINIT);
	else						/* INS = L (Card inserted) */
		s &= ~STA_NODISK;
	Stat = s;
}

/* End Of File ---------------------------------------------------------------*/
