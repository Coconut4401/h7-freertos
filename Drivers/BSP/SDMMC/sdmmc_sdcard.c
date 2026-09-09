/**
 * @file sdmmc_sdcard.c
 * @brief 驱动 SDMMC 控制器和 SD 卡，提供块读写及卡状态查询。
 * @details 这是 sdmmc_sdcard 模块的实现文件（Drivers/BSP/SDMMC/sdmmc_sdcard.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "sdmmc_sdcard.h"
#include <stdio.h>
#include <string.h>

static u8 CardType=STD_CAPACITY_SD_CARD_V1_1;
static u32 CSD_Tab[4],CID_Tab[4],RCA=0;
SD_CardInfo SDCardInfo;

__attribute__((aligned(4))) u8 SDMMC_DATA_BUFFER[512];

/**
 * @brief SD_Init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_Init(void)
{
	SD_Error errorstatus=SD_OK;
	u8 clkdiv=0;

    RCC->AHB4ENR |= (1U << 2) | (1U << 3);
    RCC->AHB3ENR |= 1U << 16;

    sys_gpio_set(GPIOC,
                 SYS_GPIO_PIN8 | SYS_GPIO_PIN9 | SYS_GPIO_PIN10 |
                 SYS_GPIO_PIN11 | SYS_GPIO_PIN12,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP,
                 SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_NONE);
    sys_gpio_af_set(GPIOC,
                    SYS_GPIO_PIN8 | SYS_GPIO_PIN9 | SYS_GPIO_PIN10 |
                    SYS_GPIO_PIN11 | SYS_GPIO_PIN12, 12U);
    sys_gpio_set(GPIOD, SYS_GPIO_PIN2, SYS_GPIO_MODE_AF,
                 SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH,
                 SYS_GPIO_PUPD_NONE);
    sys_gpio_af_set(GPIOD, SYS_GPIO_PIN2, 12U);

	SDMMC1->POWER=0x00000000;
	SDMMC1->CLKCR=0x00000000;
	SDMMC1->ARG=0x00000000;
	SDMMC1->CMD=0x00000000;
	SDMMC1->DTIMER=0x00000000;
	SDMMC1->DLEN=0x00000000;
	SDMMC1->DCTRL=0x00000000;
	SDMMC1->ICR=0X1FE00FFF;
	SDMMC1->MASK=0x00000000;

   	errorstatus=SD_PowerON();
 	if(errorstatus==SD_OK)errorstatus=SD_InitializeCards();
  	if(errorstatus==SD_OK)errorstatus=SD_GetCardInfo(&SDCardInfo);
 	if(errorstatus==SD_OK)errorstatus=SD_SelectDeselect((u32)(SDCardInfo.RCA<<16));
   	if(errorstatus==SD_OK)errorstatus=SD_EnableWideBusOperation(1);
  	if((errorstatus==SD_OK)||(MULTIMEDIA_CARD==CardType))
	{
		if(SDCardInfo.CardType==STD_CAPACITY_SD_CARD_V1_1||SDCardInfo.CardType==STD_CAPACITY_SD_CARD_V2_0)
		{
			clkdiv=SDMMC_TRANSFER_CLK_DIV+2;
		}else clkdiv=SDMMC_TRANSFER_CLK_DIV;
		SDMMC_Clock_Set(clkdiv);
  	}
	return errorstatus;
}

/**
 * @brief SDMMC_Clock_Set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param clkdiv 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void SDMMC_Clock_Set(u16 clkdiv)
{
	u32 tmpreg=SDMMC1->CLKCR;
  	tmpreg&=0XFFFFFC00;
 	tmpreg|=clkdiv;
	SDMMC1->CLKCR=tmpreg;
}

/**
 * @brief SDMMC_Send_Cmd：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmdindex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param waitrsp 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param arg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void SDMMC_Send_Cmd(u8 cmdindex,u8 waitrsp,u32 arg)
{
	u32 tmpreg=0;
	SDMMC1->ARG=arg;
	tmpreg|=cmdindex&0X3F;
	tmpreg|=(u32)waitrsp<<8;
	tmpreg|=0<<10;
  	tmpreg|=1<<12;
	SDMMC1->CMD=tmpreg;
}

/**
 * @brief SDMMC_Send_Data_Cfg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datatimeout 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param datalen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param blksize 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void SDMMC_Send_Data_Cfg(u32 datatimeout,u32 datalen,u8 blksize,u8 dir)
{
	u32 tmpreg;
	SDMMC1->DTIMER=datatimeout;
  	SDMMC1->DLEN=datalen&0X1FFFFFF;
	tmpreg=SDMMC1->DCTRL;
	tmpreg&=0xFFFFFF00;
	tmpreg|=blksize<<4;
	tmpreg|=0<<2;
	tmpreg|=(dir&0X01)<<1;
	tmpreg|=1<<0;
	SDMMC1->DCTRL=tmpreg;
}

/**
 * @brief SD_PowerON：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_PowerON(void)
{
 	u8 i=0;
	u32 tempreg=0;
	SD_Error errorstatus=SD_OK;
	u32 response=0,count=0,validvoltage=0;
	u32 SDType=SD_STD_CAPACITY;

	tempreg|=0<<12;
	tempreg|=0<<14;
	tempreg|=0<<16;
	tempreg|=0<<17;
	SDMMC1->CLKCR=tempreg;
	SDMMC_Clock_Set(SDMMC_INIT_CLK_DIV);
 	SDMMC1->POWER=0X03;
   	for(i=0;i<74;i++)
	{
		SDMMC_Send_Cmd(SD_CMD_GO_IDLE_STATE,0,0);
		errorstatus=CmdError();
		if(errorstatus==SD_OK)break;
 	}
 	if(errorstatus)return errorstatus;
	SDMMC_Send_Cmd(SD_SDMMC_SEND_IF_COND,1,SD_CHECK_PATTERN);

  	errorstatus=CmdResp7Error();
 	if(errorstatus==SD_OK)
	{
		CardType=STD_CAPACITY_SD_CARD_V2_0;
		SDType=SD_HIGH_CAPACITY;
	}
	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,0);
	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
	if(errorstatus==SD_OK)
	{

		while((!validvoltage)&&(count<SD_MAX_VOLT_TRIAL))
		{
			SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,0);
			errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
 			if(errorstatus!=SD_OK)return errorstatus;
			SDMMC_Send_Cmd(SD_CMD_SD_APP_OP_COND,1,SD_VOLTAGE_WINDOW_SD|SDType);
			errorstatus=CmdResp3Error();
 			if(errorstatus!=SD_OK)return errorstatus;
			response=SDMMC1->RESP1;;
			validvoltage=(((response>>31)==1)?1:0);
			count++;
		}
		if(count>=SD_MAX_VOLT_TRIAL)
		{
			errorstatus=SD_INVALID_VOLTRANGE;
			return errorstatus;
		}
		if(response&=SD_HIGH_CAPACITY)
		{
			CardType=HIGH_CAPACITY_SD_CARD;
		}
 	}else
	{

		while((!validvoltage)&&(count<SD_MAX_VOLT_TRIAL))
		{
			SDMMC_Send_Cmd(SD_CMD_SEND_OP_COND,1,SD_VOLTAGE_WINDOW_MMC);
			errorstatus=CmdResp3Error();
 			if(errorstatus!=SD_OK)return errorstatus;
			response=SDMMC1->RESP1;;
			validvoltage=(((response>>31)==1)?1:0);
			count++;
		}
		if(count>=SD_MAX_VOLT_TRIAL)
		{
			errorstatus=SD_INVALID_VOLTRANGE;
			return errorstatus;
		}
		CardType=MULTIMEDIA_CARD;
  	}
  	return(errorstatus);
}

/**
 * @brief SD_PowerOFF：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_PowerOFF(void)
{
  	SDMMC1->POWER&=~(3<<0);
	return SD_OK;
}

/**
 * @brief SD_InitializeCards：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_InitializeCards(void)
{
 	SD_Error errorstatus=SD_OK;
	u16 rca = 0x01;
 	if((SDMMC1->POWER&0X03)==0)return SD_REQUEST_NOT_APPLICABLE;
 	if(SECURE_DIGITAL_IO_CARD!=CardType)
	{
		SDMMC_Send_Cmd(SD_CMD_ALL_SEND_CID,3,0);
		errorstatus=CmdResp2Error();
		if(errorstatus!=SD_OK)return errorstatus;
 		CID_Tab[0]=SDMMC1->RESP1;
		CID_Tab[1]=SDMMC1->RESP2;
		CID_Tab[2]=SDMMC1->RESP3;
		CID_Tab[3]=SDMMC1->RESP4;
	}
	if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(SECURE_DIGITAL_IO_COMBO_CARD==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
	{
		SDMMC_Send_Cmd(SD_CMD_SET_REL_ADDR,1,0);
		errorstatus=CmdResp6Error(SD_CMD_SET_REL_ADDR,&rca);
		if(errorstatus!=SD_OK)return errorstatus;
	}
    if (MULTIMEDIA_CARD==CardType)
    {
 		SDMMC_Send_Cmd(SD_CMD_SET_REL_ADDR,1,(u32)(rca<<16));
		errorstatus=CmdResp2Error();
		if(errorstatus!=SD_OK)return errorstatus;
    }
	if (SECURE_DIGITAL_IO_CARD!=CardType)
	{
		RCA = rca;
		SDMMC_Send_Cmd(SD_CMD_SEND_CSD,3,(u32)(rca<<16));
		errorstatus=CmdResp2Error();
		if(errorstatus!=SD_OK)return errorstatus;
  		CSD_Tab[0]=SDMMC1->RESP1;
		CSD_Tab[1]=SDMMC1->RESP2;
		CSD_Tab[2]=SDMMC1->RESP3;
		CSD_Tab[3]=SDMMC1->RESP4;
	}
	return SD_OK;
}

/**
 * @brief SD_GetCardInfo：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cardinfo 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_GetCardInfo(SD_CardInfo *cardinfo)
{
 	SD_Error errorstatus=SD_OK;
	u8 tmp=0;
	cardinfo->CardType=(u8)CardType;
	cardinfo->RCA=(u16)RCA;
	tmp=(u8)((CSD_Tab[0]&0xFF000000)>>24);
	cardinfo->SD_csd.CSDStruct=(tmp&0xC0)>>6;
	cardinfo->SD_csd.SysSpecVersion=(tmp&0x3C)>>2;
	cardinfo->SD_csd.Reserved1=tmp&0x03;
	tmp=(u8)((CSD_Tab[0]&0x00FF0000)>>16);
	cardinfo->SD_csd.TAAC=tmp;
	tmp=(u8)((CSD_Tab[0]&0x0000FF00)>>8);
	cardinfo->SD_csd.NSAC=tmp;
	tmp=(u8)(CSD_Tab[0]&0x000000FF);
	cardinfo->SD_csd.MaxBusClkFrec=tmp;
	tmp=(u8)((CSD_Tab[1]&0xFF000000)>>24);
	cardinfo->SD_csd.CardComdClasses=tmp<<4;
	tmp=(u8)((CSD_Tab[1]&0x00FF0000)>>16);
	cardinfo->SD_csd.CardComdClasses|=(tmp&0xF0)>>4;
	cardinfo->SD_csd.RdBlockLen=tmp&0x0F;
	tmp=(u8)((CSD_Tab[1]&0x0000FF00)>>8);
	cardinfo->SD_csd.PartBlockRead=(tmp&0x80)>>7;
	cardinfo->SD_csd.WrBlockMisalign=(tmp&0x40)>>6;
	cardinfo->SD_csd.RdBlockMisalign=(tmp&0x20)>>5;
	cardinfo->SD_csd.DSRImpl=(tmp&0x10)>>4;
	cardinfo->SD_csd.Reserved2=0;
 	if((CardType==STD_CAPACITY_SD_CARD_V1_1)||(CardType==STD_CAPACITY_SD_CARD_V2_0)||(MULTIMEDIA_CARD==CardType))
	{
		cardinfo->SD_csd.DeviceSize=(tmp&0x03)<<10;
	 	tmp=(u8)(CSD_Tab[1]&0x000000FF);
		cardinfo->SD_csd.DeviceSize|=(tmp)<<2;
 		tmp=(u8)((CSD_Tab[2]&0xFF000000)>>24);
		cardinfo->SD_csd.DeviceSize|=(tmp&0xC0)>>6;
 		cardinfo->SD_csd.MaxRdCurrentVDDMin=(tmp&0x38)>>3;
		cardinfo->SD_csd.MaxRdCurrentVDDMax=(tmp&0x07);
 		tmp=(u8)((CSD_Tab[2]&0x00FF0000)>>16);
		cardinfo->SD_csd.MaxWrCurrentVDDMin=(tmp&0xE0)>>5;
		cardinfo->SD_csd.MaxWrCurrentVDDMax=(tmp&0x1C)>>2;
		cardinfo->SD_csd.DeviceSizeMul=(tmp&0x03)<<1;
 		tmp=(u8)((CSD_Tab[2]&0x0000FF00)>>8);
		cardinfo->SD_csd.DeviceSizeMul|=(tmp&0x80)>>7;
 		cardinfo->CardCapacity=(cardinfo->SD_csd.DeviceSize+1);
		cardinfo->CardCapacity*=(1<<(cardinfo->SD_csd.DeviceSizeMul+2));
		cardinfo->CardBlockSize=1<<(cardinfo->SD_csd.RdBlockLen);
		cardinfo->CardCapacity*=cardinfo->CardBlockSize;
	}else if(CardType==HIGH_CAPACITY_SD_CARD)
	{
 		tmp=(u8)(CSD_Tab[1]&0x000000FF);
		cardinfo->SD_csd.DeviceSize=(tmp&0x3F)<<16;
 		tmp=(u8)((CSD_Tab[2]&0xFF000000)>>24);
 		cardinfo->SD_csd.DeviceSize|=(tmp<<8);
 		tmp=(u8)((CSD_Tab[2]&0x00FF0000)>>16);
 		cardinfo->SD_csd.DeviceSize|=(tmp);
 		tmp=(u8)((CSD_Tab[2]&0x0000FF00)>>8);
 		cardinfo->CardCapacity=(long long)(cardinfo->SD_csd.DeviceSize+1)*512*1024;
		cardinfo->CardBlockSize=512;
	}
	cardinfo->SD_csd.EraseGrSize=(tmp&0x40)>>6;
	cardinfo->SD_csd.EraseGrMul=(tmp&0x3F)<<1;
	tmp=(u8)(CSD_Tab[2]&0x000000FF);
	cardinfo->SD_csd.EraseGrMul|=(tmp&0x80)>>7;
	cardinfo->SD_csd.WrProtectGrSize=(tmp&0x7F);
 	tmp=(u8)((CSD_Tab[3]&0xFF000000)>>24);
	cardinfo->SD_csd.WrProtectGrEnable=(tmp&0x80)>>7;
	cardinfo->SD_csd.ManDeflECC=(tmp&0x60)>>5;
	cardinfo->SD_csd.WrSpeedFact=(tmp&0x1C)>>2;
	cardinfo->SD_csd.MaxWrBlockLen=(tmp&0x03)<<2;
	tmp=(u8)((CSD_Tab[3]&0x00FF0000)>>16);
	cardinfo->SD_csd.MaxWrBlockLen|=(tmp&0xC0)>>6;
	cardinfo->SD_csd.WriteBlockPaPartial=(tmp&0x20)>>5;
	cardinfo->SD_csd.Reserved3=0;
	cardinfo->SD_csd.ContentProtectAppli=(tmp&0x01);
	tmp=(u8)((CSD_Tab[3]&0x0000FF00)>>8);
	cardinfo->SD_csd.FileFormatGrouop=(tmp&0x80)>>7;
	cardinfo->SD_csd.CopyFlag=(tmp&0x40)>>6;
	cardinfo->SD_csd.PermWrProtect=(tmp&0x20)>>5;
	cardinfo->SD_csd.TempWrProtect=(tmp&0x10)>>4;
	cardinfo->SD_csd.FileFormat=(tmp&0x0C)>>2;
	cardinfo->SD_csd.ECC=(tmp&0x03);
	tmp=(u8)(CSD_Tab[3]&0x000000FF);
	cardinfo->SD_csd.CSD_CRC=(tmp&0xFE)>>1;
	cardinfo->SD_csd.Reserved4=1;
	tmp=(u8)((CID_Tab[0]&0xFF000000)>>24);
	cardinfo->SD_cid.ManufacturerID=tmp;
	tmp=(u8)((CID_Tab[0]&0x00FF0000)>>16);
	cardinfo->SD_cid.OEM_AppliID=tmp<<8;
	tmp=(u8)((CID_Tab[0]&0x000000FF00)>>8);
	cardinfo->SD_cid.OEM_AppliID|=tmp;
	tmp=(u8)(CID_Tab[0]&0x000000FF);
	cardinfo->SD_cid.ProdName1=tmp<<24;
	tmp=(u8)((CID_Tab[1]&0xFF000000)>>24);
	cardinfo->SD_cid.ProdName1|=tmp<<16;
	tmp=(u8)((CID_Tab[1]&0x00FF0000)>>16);
	cardinfo->SD_cid.ProdName1|=tmp<<8;
	tmp=(u8)((CID_Tab[1]&0x0000FF00)>>8);
	cardinfo->SD_cid.ProdName1|=tmp;
	tmp=(u8)(CID_Tab[1]&0x000000FF);
	cardinfo->SD_cid.ProdName2=tmp;
	tmp=(u8)((CID_Tab[2]&0xFF000000)>>24);
	cardinfo->SD_cid.ProdRev=tmp;
	tmp=(u8)((CID_Tab[2]&0x00FF0000)>>16);
	cardinfo->SD_cid.ProdSN=tmp<<24;
	tmp=(u8)((CID_Tab[2]&0x0000FF00)>>8);
	cardinfo->SD_cid.ProdSN|=tmp<<16;
	tmp=(u8)(CID_Tab[2]&0x000000FF);
	cardinfo->SD_cid.ProdSN|=tmp<<8;
	tmp=(u8)((CID_Tab[3]&0xFF000000)>>24);
	cardinfo->SD_cid.ProdSN|=tmp;
	tmp=(u8)((CID_Tab[3]&0x00FF0000)>>16);
	cardinfo->SD_cid.Reserved1|=(tmp&0xF0)>>4;
	cardinfo->SD_cid.ManufactDate=(tmp&0x0F)<<8;
	tmp=(u8)((CID_Tab[3]&0x0000FF00)>>8);
	cardinfo->SD_cid.ManufactDate|=tmp;
	tmp=(u8)(CID_Tab[3]&0x000000FF);
	cardinfo->SD_cid.CID_CRC=(tmp&0xFE)>>1;
	cardinfo->SD_cid.Reserved2=1;
	return errorstatus;
}

/**
 * @brief SD_EnableWideBusOperation：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param wmode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_EnableWideBusOperation(u32 wmode)
{
  	SD_Error errorstatus=SD_OK;
	u16 clkcr=0;
  	if(MULTIMEDIA_CARD==CardType)return SD_UNSUPPORTED_FEATURE;
 	else if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
	{
		if(wmode>=2)return SD_UNSUPPORTED_FEATURE;
 		else
		{
			errorstatus=SDEnWideBus(wmode);
 			if(SD_OK==errorstatus)
			{
				clkcr=SDMMC1->CLKCR;
				clkcr&=~(3<<14);
				clkcr|=(u32)wmode<<14;
				clkcr|=0<<17;
				SDMMC1->CLKCR=clkcr;
			}
		}
	}
	return errorstatus;
}

/**
 * @brief SD_SelectDeselect：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_SelectDeselect(u32 addr)
{
 	SDMMC_Send_Cmd(SD_CMD_SEL_DESEL_CARD,1,addr);
   	return CmdResp1Error(SD_CMD_SEL_DESEL_CARD);
}

/**
 * @brief SD_ReadBlocks：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param blksize 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param nblks 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_ReadBlocks(u8 *buf,long long addr,u16 blksize,u32 nblks)
{
  	SD_Error errorstatus=SD_OK;
   	u32 count=0;
	u32 timeout=SDMMC_DATATIMEOUT;
	u32 *tempbuff=(u32*)buf;
    SDMMC1->DCTRL=0x0;
	if(CardType==HIGH_CAPACITY_SD_CARD)
	{
		blksize=512;
		addr>>=9;
	}
	SDMMC_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);
	errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);
	if(errorstatus!=SD_OK)
    {
        printf("SDMMC_Send_Cmd=%d\r\n",errorstatus);
        return errorstatus;
    }
	SDMMC_Send_Data_Cfg(SD_DATATIMEOUT,nblks*blksize,9,1);
	SDMMC1->CMD|=1<<6;
	if(nblks>1)
	{
		SDMMC_Send_Cmd(SD_CMD_READ_MULT_BLOCK,1,addr);
		errorstatus=CmdResp1Error(SD_CMD_READ_MULT_BLOCK);
		if(errorstatus!=SD_OK)
		{
			printf("SD_CMD_READ_MULT_BLOCK Error\r\n");
			return errorstatus;
		}
	}else
	{
		SDMMC_Send_Cmd(SD_CMD_READ_SINGLE_BLOCK,1,addr);
		errorstatus=CmdResp1Error(SD_CMD_READ_SINGLE_BLOCK);
		if(errorstatus!=SD_OK)return errorstatus;
	}
	sys_intx_disable();
	while(!(SDMMC1->STA&((1<<5)|(1<<1)|(1<<3)|(1<<8))))
	{
		if(SDMMC1->STA&(1<<15))
		{
			for(count=0;count<8;count++)
			{
				*(tempbuff+count)=SDMMC1->FIFO;
			}
			tempbuff+=8;
			timeout=0X7FFFFF;
		}else
		{
			if(timeout==0)return SD_DATA_TIMEOUT;
			timeout--;
		}
	}
	SDMMC1->CMD&=~(1<<6);
	sys_intx_enable();
	if(SDMMC1->STA&(1<<3))
	{
		SDMMC1->ICR|=1<<3;
		return SD_DATA_TIMEOUT;
	}else if(SDMMC1->STA&(1<<1))
	{
		SDMMC1->ICR|=1<<1;
		if(nblks>1)
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);
		}
		return SD_DATA_CRC_FAIL;
	}else if(SDMMC1->STA&(1<<5))
	{
		SDMMC1->ICR|=1<<5;
		return SD_RX_OVERRUN;
	}
	if((SDMMC1->STA&(1<<8))&&(nblks>1))
	{
		if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);
			if(errorstatus!=SD_OK)return errorstatus;
		}
	}
	SDMMC1->ICR=0X1FE00FFF;
	return errorstatus;
}

/**
 * @brief SD_WriteBlocks：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param blksize 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param nblks 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_WriteBlocks(u8 *buf,long long addr,u16 blksize,u32 nblks)
{
	SD_Error errorstatus = SD_OK;
	u8  cardstate=0;
	u32 timeout=0,bytestransferred=0;
	u32 cardstatus=0,count=0,restwords=0;
	u32 tlen=nblks*blksize;
	u32*tempbuff=(u32*)buf;
 	if(buf==NULL)return SD_INVALID_PARAMETER;
  	SDMMC1->DCTRL=0x0;
 	if(CardType==HIGH_CAPACITY_SD_CARD)
	{
		blksize=512;
		addr>>=9;
	}
	SDMMC_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);
	errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);
	if(errorstatus!=SD_OK)return errorstatus;
	if(nblks>1)
	{
		if(nblks*blksize>SD_MAX_DATA_LENGTH)return SD_INVALID_PARAMETER;
     	if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
    	{

	 	   	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,(u32)RCA<<16);
			errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
			if(errorstatus!=SD_OK)return errorstatus;
	 	   	SDMMC_Send_Cmd(SD_CMD_SET_BLOCK_COUNT,1,nblks);
			errorstatus=CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);
			if(errorstatus!=SD_OK)return errorstatus;
		}
		SDMMC_Send_Cmd(SD_CMD_WRITE_MULT_BLOCK,1,addr);
		errorstatus=CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);
	}else
	{
		SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);
		errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);
		if(errorstatus!=SD_OK)return errorstatus;
		cardstatus=SDMMC1->RESP1;
		timeout=SD_DATATIMEOUT;
		while(((cardstatus&0x00000100)==0)&&(timeout>0))
		{
			timeout--;
			SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);
			errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);
			if(errorstatus!=SD_OK)return errorstatus;
			cardstatus=SDMMC1->RESP1;
		}
		if(timeout==0)return SD_ERROR;
		SDMMC_Send_Cmd(SD_CMD_WRITE_SINGLE_BLOCK,1,addr);
		errorstatus=CmdResp1Error(SD_CMD_WRITE_SINGLE_BLOCK);
	}
	if(errorstatus!=SD_OK)return errorstatus;
 	SDMMC_Send_Data_Cfg(SD_DATATIMEOUT,nblks*blksize,9,0);
	SDMMC1->CMD|=1<<6;
	timeout=SDMMC_DATATIMEOUT;
	sys_intx_disable();
	while(!(SDMMC1->STA&((1<<4)|(1<<1)|(1<<8)|(1<<3))))
	{
		if(SDMMC1->STA&(1<<14))
		{
			if((tlen-bytestransferred)<SD_HALFFIFOBYTES)
			{
				restwords=((tlen-bytestransferred)%4==0)?((tlen-bytestransferred)/4):((tlen-bytestransferred)/4+1);
				for(count=0;count<restwords;count++,tempbuff++,bytestransferred+=4)
				{
					SDMMC1->FIFO=*tempbuff;
				}
			}else
			{
				for(count=0;count<SD_HALFFIFO;count++)
				{
					SDMMC1->FIFO=*(tempbuff+count);
				}
				tempbuff+=SD_HALFFIFO;
				bytestransferred+=SD_HALFFIFOBYTES;
			}
			timeout=0X3FFFFFFF;
		}else
		{
			if(timeout==0)return SD_DATA_TIMEOUT;
			timeout--;
		}
	}
	SDMMC1->CMD&=~(1<<6);
	sys_intx_enable();
	if(SDMMC1->STA&(1<<3))
	{
		SDMMC1->ICR|=1<<3;
		return SD_DATA_TIMEOUT;
	}else if(SDMMC1->STA&(1<<1))
	{
		SDMMC1->ICR|=1<<1;
		if(nblks>1)
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);
		}
		return SD_DATA_CRC_FAIL;
	}else if(SDMMC1->STA&(1<<4))
	{
		SDMMC1->ICR|=1<<4;
		return SD_TX_UNDERRUN;
	}
	if((SDMMC1->STA&(1<<8))&&(nblks>1))
	{
		if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);
			if(errorstatus!=SD_OK)return errorstatus;
		}
	}
	SDMMC1->ICR=0X1FE00FFF;
 	errorstatus=IsCardProgramming(&cardstate);
 	while((errorstatus==SD_OK)&&((cardstate==SD_CARD_PROGRAMMING)||(cardstate==SD_CARD_RECEIVING)))
	{
		errorstatus=IsCardProgramming(&cardstate);
	}
	return errorstatus;
}

/**
 * @brief CmdError：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdError(void)
{
	SD_Error errorstatus = SD_OK;
	u32 timeout=SDMMC_CMD0TIMEOUT;
	while(timeout--)
	{
		if(SDMMC1->STA&(1<<7))break;
	}
	if(timeout==0)return SD_CMD_RSP_TIMEOUT;
	SDMMC1->ICR=0X1FE00FFF;
	return errorstatus;
}

/**
 * @brief CmdResp7Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp7Error(void)
{
	SD_Error errorstatus=SD_OK;
	u32 status;
	u32 timeout=SDMMC_CMD0TIMEOUT;
 	while(timeout--)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
 	if((timeout==0)||(status&(1<<2)))
	{
		errorstatus=SD_CMD_RSP_TIMEOUT;
		SDMMC1->ICR|=1<<2;
		return errorstatus;
	}
	if(status&1<<6)
	{
		errorstatus=SD_OK;
		SDMMC1->ICR|=1<<6;
 	}
	return errorstatus;
}

/**
 * @brief CmdResp1Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp1Error(u8 cmd)
{
   	u32 status;
	while(1)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
	if(status&(1<<2))
	{
 		SDMMC1->ICR=1<<2;
		SDMMC1->ICR=0X1FE00FFF;
		return SD_CMD_RSP_TIMEOUT;
	}
 	if(status&(1<<0))
	{
 		SDMMC1->ICR=1<<0;
		return SD_CMD_CRC_FAIL;
	}
	if(SDMMC1->RESPCMD!=cmd)return SD_ILLEGAL_CMD;
  	SDMMC1->ICR=0X1FE00FFF;
	return (SD_Error)(SDMMC1->RESP1&SD_OCR_ERRORBITS);
}

/**
 * @brief CmdResp3Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp3Error(void)
{
	u32 status;
 	while(1)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
 	if(status&(1<<2))
	{
		SDMMC1->ICR|=1<<2;
		return SD_CMD_RSP_TIMEOUT;
	}
   	SDMMC1->ICR=0X1FE00FFF;
 	return SD_OK;
}

/**
 * @brief CmdResp2Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp2Error(void)
{
	SD_Error errorstatus=SD_OK;
	u32 status;
	u32 timeout=SDMMC_CMD0TIMEOUT;
 	while(timeout--)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
  	if((timeout==0)||(status&(1<<2)))
	{
		errorstatus=SD_CMD_RSP_TIMEOUT;
		SDMMC1->ICR|=1<<2;
		return errorstatus;
	}
	if(status&1<<0)
	{
		errorstatus=SD_CMD_CRC_FAIL;
		SDMMC1->ICR|=1<<0;
 	}
	SDMMC1->ICR=0X1FE00FFF;
 	return errorstatus;
}

/**
 * @brief CmdResp6Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param prca 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp6Error(u8 cmd,u16*prca)
{
	SD_Error errorstatus=SD_OK;
	u32 status;
	u32 rspr1;
 	while(1)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;
	}
	if(status&(1<<2))
	{
 		SDMMC1->ICR|=1<<2;
		return SD_CMD_RSP_TIMEOUT;
	}
	if(status&1<<0)
	{
		SDMMC1->ICR|=1<<0;
 		return SD_CMD_CRC_FAIL;
	}
	if(SDMMC1->RESPCMD!=cmd)
	{
 		return SD_ILLEGAL_CMD;
	}
	SDMMC1->ICR=0X1FE00FFF;
	rspr1=SDMMC1->RESP1;
	if(SD_ALLZERO==(rspr1&(SD_R6_GENERAL_UNKNOWN_ERROR|SD_R6_ILLEGAL_CMD|SD_R6_COM_CRC_FAILED)))
	{
		*prca=(u16)(rspr1>>16);
		return errorstatus;
	}
   	if(rspr1&SD_R6_GENERAL_UNKNOWN_ERROR)return SD_GENERAL_UNKNOWN_ERROR;
   	if(rspr1&SD_R6_ILLEGAL_CMD)return SD_ILLEGAL_CMD;
   	if(rspr1&SD_R6_COM_CRC_FAILED)return SD_COM_CRC_FAILED;
	return errorstatus;
}

/**
 * @brief SDEnWideBus：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param enx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SDEnWideBus(u8 enx)
{
	SD_Error errorstatus = SD_OK;
 	u32 scr[2]={0,0};
	u8 arg=0X00;
	if(enx)arg=0X02;
	else arg=0X00;
 	if(SDMMC1->RESP1&SD_CARD_LOCKED)return SD_LOCK_UNLOCK_FAILED;
 	errorstatus=FindSCR(RCA,scr);
 	if(errorstatus!=SD_OK)return errorstatus;
	if((scr[1]&SD_WIDE_BUS_SUPPORT)!=SD_ALLZERO)
	{
	 	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,(u32)RCA<<16);
	 	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
	 	if(errorstatus!=SD_OK)return errorstatus;
	 	SDMMC_Send_Cmd(SD_CMD_APP_SD_SET_BUSWIDTH,1,arg);
		errorstatus=CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);
		return errorstatus;
	}else return SD_REQUEST_NOT_APPLICABLE;
}

/**
 * @brief IsCardProgramming：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pstatus 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error IsCardProgramming(u8 *pstatus)
{
 	vu32 respR1 = 0, status = 0;
  	SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);
  	status=SDMMC1->STA;
	while(!(status&((1<<0)|(1<<6)|(1<<2))))status=SDMMC1->STA;
   	if(status&(1<<0))
	{
		SDMMC1->ICR|=1<<0;
		return SD_CMD_CRC_FAIL;
	}
   	if(status&(1<<2))
	{
		SDMMC1->ICR|=1<<2;
		return SD_CMD_RSP_TIMEOUT;
	}
 	if(SDMMC1->RESPCMD!=SD_CMD_SEND_STATUS)return SD_ILLEGAL_CMD;
	SDMMC1->ICR=0X1FE00FFF;
	respR1=SDMMC1->RESP1;
	*pstatus=(u8)((respR1>>9)&0x0000000F);
	return SD_OK;
}

/**
 * @brief SD_SendStatus：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pcardstatus 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_SendStatus(uint32_t *pcardstatus)
{
	SD_Error errorstatus = SD_OK;
	if(pcardstatus==NULL)
	{
		errorstatus=SD_INVALID_PARAMETER;
		return errorstatus;
	}
 	SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,RCA<<16);
	errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);
	if(errorstatus!=SD_OK)return errorstatus;
	*pcardstatus=SDMMC1->RESP1;
	return errorstatus;
}

/**
 * @brief SD_GetState：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SDCardState SD_GetState(void)
{
	u32 resp1=0;
	if(SD_SendStatus(&resp1)!=SD_OK)return SD_CARD_ERROR;
	else return (SDCardState)((resp1>>9) & 0x0F);
}

/**
 * @brief FindSCR：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param rca 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pscr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error FindSCR(u16 rca,u32 *pscr)
{
	SD_Error errorstatus = SD_OK;
	u32 tempscr[2]={0,0};
 	SDMMC_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,8);
 	errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);
 	if(errorstatus!=SD_OK)return errorstatus;
  	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,(u32)rca<<16);
 	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
 	if(errorstatus!=SD_OK)return errorstatus;
	SDMMC_Send_Data_Cfg(SD_DATATIMEOUT,8,3,1);
   	SDMMC_Send_Cmd(SD_CMD_SD_APP_SEND_SCR,1,0);
 	errorstatus=CmdResp1Error(SD_CMD_SD_APP_SEND_SCR);
 	if(errorstatus!=SD_OK)return errorstatus;
 	while(!(SDMMC1->STA&(SDMMC_STA_RXOVERR|SDMMC_STA_DCRCFAIL|SDMMC_STA_DTIMEOUT|SDMMC_STA_DBCKEND|SDMMC_STA_DATAEND)))
	{
		if(!(SDMMC1->STA&(1<<19)))
		{
			tempscr[0]=SDMMC1->FIFO;
			tempscr[1]=SDMMC1->FIFO;
			break;
		}
	}
 	if(SDMMC1->STA&(1<<3))
	{
 		SDMMC1->ICR|=1<<3;
		return SD_DATA_TIMEOUT;
	}else if(SDMMC1->STA&(1<<1))
	{
 		SDMMC1->ICR|=1<<1;
		return SD_DATA_CRC_FAIL;
	}else if(SDMMC1->STA&(1<<5))
	{
 		SDMMC1->ICR|=1<<5;
		return SD_RX_OVERRUN;
	}
   	SDMMC1->ICR=0X1FE00FFF;

	*(pscr+1)=((tempscr[0]&SD_0TO7BITS)<<24)|((tempscr[0]&SD_8TO15BITS)<<8)|((tempscr[0]&SD_16TO23BITS)>>8)|((tempscr[0]&SD_24TO31BITS)>>24);
	*(pscr)=((tempscr[1]&SD_0TO7BITS)<<24)|((tempscr[1]&SD_8TO15BITS)<<8)|((tempscr[1]&SD_16TO23BITS)>>8)|((tempscr[1]&SD_24TO31BITS)>>24);
 	return errorstatus;
}

/**
 * @brief SD_ReadDisk：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sector 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cnt 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
u8 SD_ReadDisk(u8*buf,u32 sector,u32 cnt)
{
	u8 sta=SD_OK;
	long long lsector=sector;
	u32 n;
	if(CardType!=STD_CAPACITY_SD_CARD_V1_1)lsector<<=9;
	if((u32)buf%4!=0)
	{
	 	for(n=0;n<cnt;n++)
		{
		 	sta=SD_ReadBlocks(SDMMC_DATA_BUFFER,lsector+512*n,512,1);
			memcpy(buf,SDMMC_DATA_BUFFER,512);
			buf+=512;
		}
	}else sta=SD_ReadBlocks(buf,lsector,512,cnt);
	return sta;
}

/**
 * @brief SD_WriteDisk：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sector 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cnt 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
u8 SD_WriteDisk(u8*buf,u32 sector,u32 cnt)
{
	u8 sta=SD_OK;
	u32 n;
	long long lsector=sector;
	if(CardType!=STD_CAPACITY_SD_CARD_V1_1)lsector<<=9;
	if((u32)buf%4!=0)
	{
	 	for(n=0;n<cnt;n++)
		{
			memcpy(SDMMC_DATA_BUFFER,buf,512);
		 	sta=SD_WriteBlocks(SDMMC_DATA_BUFFER,lsector+512*n,512,1);
			buf+=512;
		}
	}else sta=SD_WriteBlocks(buf,lsector,512,cnt);
	return sta;
}
