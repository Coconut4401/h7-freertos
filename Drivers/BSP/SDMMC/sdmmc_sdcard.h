/**
 * @file sdmmc_sdcard.h
 * @brief 驱动 SDMMC 控制器和 SD 卡，提供块读写及卡状态查询。
 * @details 这是 sdmmc_sdcard 模块的接口文件（Drivers/BSP/SDMMC/sdmmc_sdcard.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __SDMMC_SDCARD_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __SDMMC_SDCARD_H
#include "./SYSTEM/sys/sys.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef volatile uint32_t vu32;

#define SDMMC_INIT_CLK_DIV        0xFAU
#define SDMMC_TRANSFER_CLK_DIV    0x04

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{

	SD_CMD_CRC_FAIL                    = (1),
	SD_DATA_CRC_FAIL                   = (2),
	SD_CMD_RSP_TIMEOUT                 = (3),
	SD_DATA_TIMEOUT                    = (4),
	SD_TX_UNDERRUN                     = (5),
	SD_RX_OVERRUN                      = (6),
	SD_START_BIT_ERR                   = (7),
	SD_CMD_OUT_OF_RANGE                = (8),
	SD_ADDR_MISALIGNED                 = (9),
	SD_BLOCK_LEN_ERR                   = (10),
	SD_ERASE_SEQ_ERR                   = (11),
	SD_BAD_ERASE_PARAM                 = (12),
	SD_WRITE_PROT_VIOLATION            = (13),
	SD_LOCK_UNLOCK_FAILED              = (14),
	SD_COM_CRC_FAILED                  = (15),
	SD_ILLEGAL_CMD                     = (16),
	SD_CARD_ECC_FAILED                 = (17),
	SD_CC_ERROR                        = (18),
	SD_GENERAL_UNKNOWN_ERROR           = (19),
	SD_STREAM_READ_UNDERRUN            = (20),
	SD_STREAM_WRITE_OVERRUN            = (21),
	SD_CID_CSD_OVERWRITE               = (22),
	SD_WP_ERASE_SKIP                   = (23),
	SD_CARD_ECC_DISABLED               = (24),
	SD_ERASE_RESET                     = (25),
	SD_AKE_SEQ_ERROR                   = (26),
	SD_INVALID_VOLTRANGE               = (27),
	SD_ADDR_OUT_OF_RANGE               = (28),
	SD_SWITCH_ERROR                    = (29),
	SD_SDMMC_DISABLED                  = (30),
	SD_SDMMC_FUNCTION_BUSY             = (31),
	SD_SDMMC_FUNCTION_FAILED           = (32),
	SD_SDMMC_UNKNOWN_FUNCTION          = (33),

	SD_INTERNAL_ERROR                  = (34),
	SD_NOT_CONFIGURED                  = (35),
	SD_REQUEST_PENDING                 = (36),
	SD_REQUEST_NOT_APPLICABLE          = (37),
	SD_INVALID_PARAMETER               = (38),
	SD_UNSUPPORTED_FEATURE             = (39),
	SD_UNSUPPORTED_HW                  = (40),
	SD_ERROR                           = (41),
	SD_OK                              = (0)
} SD_Error;

typedef struct
{
	u8  CSDStruct;
	u8  SysSpecVersion;
	u8  Reserved1;
	u8  TAAC;
	u8  NSAC;
	u8  MaxBusClkFrec;
	u16 CardComdClasses;
	u8  RdBlockLen;
	u8  PartBlockRead;
	u8  WrBlockMisalign;
	u8  RdBlockMisalign;
	u8  DSRImpl;
	u8  Reserved2;
	u32 DeviceSize;
	u8  MaxRdCurrentVDDMin;
	u8  MaxRdCurrentVDDMax;
	u8  MaxWrCurrentVDDMin;
	u8  MaxWrCurrentVDDMax;
	u8  DeviceSizeMul;
	u8  EraseGrSize;
	u8  EraseGrMul;
	u8  WrProtectGrSize;
	u8  WrProtectGrEnable;
	u8  ManDeflECC;
	u8  WrSpeedFact;
	u8  MaxWrBlockLen;
	u8  WriteBlockPaPartial;
	u8  Reserved3;
	u8  ContentProtectAppli;
	u8  FileFormatGrouop;
	u8  CopyFlag;
	u8  PermWrProtect;
	u8  TempWrProtect;
	u8  FileFormat;
	u8  ECC;
	u8  CSD_CRC;
	u8  Reserved4;
} SD_CSD;

typedef struct
{
	u8  ManufacturerID;
	u16 OEM_AppliID;
	u32 ProdName1;
	u8  ProdName2;
	u8  ProdRev;
	u32 ProdSN;
	u8  Reserved1;
	u16 ManufactDate;
	u8  CID_CRC;
	u8  Reserved2;
} SD_CID;

typedef enum
{
	SD_CARD_READY                  = ((uint32_t)0x00000001),
	SD_CARD_IDENTIFICATION         = ((uint32_t)0x00000002),
	SD_CARD_STANDBY                = ((uint32_t)0x00000003),
	SD_CARD_TRANSFER               = ((uint32_t)0x00000004),
	SD_CARD_SENDING                = ((uint32_t)0x00000005),
	SD_CARD_RECEIVING              = ((uint32_t)0x00000006),
	SD_CARD_PROGRAMMING            = ((uint32_t)0x00000007),
	SD_CARD_DISCONNECTED           = ((uint32_t)0x00000008),
	SD_CARD_ERROR                  = ((uint32_t)0x000000FF)
}SDCardState;

typedef struct
{
  SD_CSD SD_csd;
  SD_CID SD_cid;
  long long CardCapacity;
  u32 CardBlockSize;
  u16 RCA;
  u8 CardType;
} SD_CardInfo;
extern SD_CardInfo SDCardInfo;

#define SD_CMD_GO_IDLE_STATE                       ((uint8_t)0U)
#define SD_CMD_SEND_OP_COND                        ((uint8_t)1U)
#define SD_CMD_ALL_SEND_CID                        ((uint8_t)2U)
#define SD_CMD_SET_REL_ADDR                        ((uint8_t)3U)
#define SD_CMD_SET_DSR                             ((uint8_t)4U)
#define SD_CMD_SDMMC_SEN_OP_COND                   ((uint8_t)5U)

#define SD_CMD_HS_SWITCH                           ((uint8_t)6U)
#define SD_CMD_SEL_DESEL_CARD                      ((uint8_t)7U)
#define SD_CMD_HS_SEND_EXT_CSD                     ((uint8_t)8U)

#define SD_CMD_SEND_CSD                            ((uint8_t)9U)
#define SD_CMD_SEND_CID                            ((uint8_t)10U)
#define SD_CMD_READ_DAT_UNTIL_STOP                 ((uint8_t)11U)
#define SD_CMD_STOP_TRANSMISSION                   ((uint8_t)12U)
#define SD_CMD_SEND_STATUS                         ((uint8_t)13U)
#define SD_CMD_HS_BUSTEST_READ                     ((uint8_t)14U)
#define SD_CMD_GO_INACTIVE_STATE                   ((uint8_t)15U)
#define SD_CMD_SET_BLOCKLEN                        ((uint8_t)16U)

#define SD_CMD_READ_SINGLE_BLOCK                   ((uint8_t)17U)

#define SD_CMD_READ_MULT_BLOCK                     ((uint8_t)18U)

#define SD_CMD_HS_BUSTEST_WRITE                    ((uint8_t)19U)
#define SD_CMD_WRITE_DAT_UNTIL_STOP                ((uint8_t)20U)
#define SD_CMD_SET_BLOCK_COUNT                     ((uint8_t)23U)
#define SD_CMD_WRITE_SINGLE_BLOCK                  ((uint8_t)24U)

#define SD_CMD_WRITE_MULT_BLOCK                    ((uint8_t)25U)
#define SD_CMD_PROG_CID                            ((uint8_t)26U)
#define SD_CMD_PROG_CSD                            ((uint8_t)27U)
#define SD_CMD_SET_WRITE_PROT                      ((uint8_t)28U)
#define SD_CMD_CLR_WRITE_PROT                      ((uint8_t)29U)
#define SD_CMD_SEND_WRITE_PROT                     ((uint8_t)30U)
#define SD_CMD_SD_ERASE_GRP_START                  ((uint8_t)32U)
#define SD_CMD_SD_ERASE_GRP_END                    ((uint8_t)33U)
#define SD_CMD_ERASE_GRP_START                     ((uint8_t)35U)

#define SD_CMD_ERASE_GRP_END                       ((uint8_t)36U)

#define SD_CMD_ERASE                               ((uint8_t)38U)
#define SD_CMD_FAST_IO                             ((uint8_t)39U)
#define SD_CMD_GO_IRQ_STATE                        ((uint8_t)40U)
#define SD_CMD_LOCK_UNLOCK                         ((uint8_t)42U)

#define SD_CMD_APP_CMD                             ((uint8_t)55U)

#define SD_CMD_GEN_CMD                             ((uint8_t)56U)

#define SD_CMD_NO_CMD                              ((uint8_t)64U)

#define SD_CMD_APP_SD_SET_BUSWIDTH                 ((uint8_t)6U)

#define SD_CMD_SD_APP_STATUS                       ((uint8_t)13U)
#define SD_CMD_SD_APP_SEND_NUM_WRITE_BLOCKS        ((uint8_t)22U)

#define SD_CMD_SD_APP_OP_COND                      ((uint8_t)41U)

#define SD_CMD_SD_APP_SET_CLR_CARD_DETECT          ((uint8_t)42U)
#define SD_CMD_SD_APP_SEND_SCR                     ((uint8_t)51U)
#define SD_CMD_SDMMC_RW_DIRECT                     ((uint8_t)52U)
#define SD_CMD_SDMMC_RW_EXTENDED                   ((uint8_t)53U)

#define SD_CMD_SD_APP_GET_MKB                      ((uint8_t)43U)
#define SD_CMD_SD_APP_GET_MID                      ((uint8_t)44U)
#define SD_CMD_SD_APP_SET_CER_RN1                  ((uint8_t)45U)
#define SD_CMD_SD_APP_GET_CER_RN2                  ((uint8_t)46U)
#define SD_CMD_SD_APP_SET_CER_RES2                 ((uint8_t)47U)
#define SD_CMD_SD_APP_GET_CER_RES1                 ((uint8_t)48U)
#define SD_CMD_SD_APP_SECURE_READ_MULTIPLE_BLOCK   ((uint8_t)18U)
#define SD_CMD_SD_APP_SECURE_WRITE_MULTIPLE_BLOCK  ((uint8_t)25U)
#define SD_CMD_SD_APP_SECURE_ERASE                 ((uint8_t)38U)
#define SD_CMD_SD_APP_CHANGE_SECURE_AREA           ((uint8_t)49U)
#define SD_CMD_SD_APP_SECURE_WRITE_MKB             ((uint8_t)48U)

#define SD_SDMMC_SEND_IF_COND           	  ((uint32_t)SD_CMD_HS_SEND_EXT_CSD)

#define STD_CAPACITY_SD_CARD_V1_1		((uint32_t)0x00000000U)
#define STD_CAPACITY_SD_CARD_V2_0		((uint32_t)0x00000001U)
#define HIGH_CAPACITY_SD_CARD			((uint32_t)0x00000002U)
#define MULTIMEDIA_CARD					((uint32_t)0x00000003U)
#define SECURE_DIGITAL_IO_CARD			((uint32_t)0x00000004U)
#define HIGH_SPEED_MULTIMEDIA_CARD		((uint32_t)0x00000005U)
#define SECURE_DIGITAL_IO_COMBO_CARD	((uint32_t)0x00000006U)
#define HIGH_CAPACITY_MMC_CARD			((uint32_t)0x00000007U)

#define SDMMC_CMD0TIMEOUT				((u32)0x00010000)
#define SDMMC_DATATIMEOUT       ((u32)0xFFFFFFFFU)

#define SD_OCR_ADDR_OUT_OF_RANGE        ((u32)0x80000000)
#define SD_OCR_ADDR_MISALIGNED          ((u32)0x40000000)
#define SD_OCR_BLOCK_LEN_ERR            ((u32)0x20000000)
#define SD_OCR_ERASE_SEQ_ERR            ((u32)0x10000000)
#define SD_OCR_BAD_ERASE_PARAM          ((u32)0x08000000)
#define SD_OCR_WRITE_PROT_VIOLATION     ((u32)0x04000000)
#define SD_OCR_LOCK_UNLOCK_FAILED       ((u32)0x01000000)
#define SD_OCR_COM_CRC_FAILED           ((u32)0x00800000)
#define SD_OCR_ILLEGAL_CMD              ((u32)0x00400000)
#define SD_OCR_CARD_ECC_FAILED          ((u32)0x00200000)
#define SD_OCR_CC_ERROR                 ((u32)0x00100000)
#define SD_OCR_GENERAL_UNKNOWN_ERROR    ((u32)0x00080000)
#define SD_OCR_STREAM_READ_UNDERRUN     ((u32)0x00040000)
#define SD_OCR_STREAM_WRITE_OVERRUN     ((u32)0x00020000)

#define SD_OCR_WP_ERASE_SKIP            ((u32)0x00008000)
#define SD_OCR_CARD_ECC_DISABLED        ((u32)0x00004000)
#define SD_OCR_ERASE_RESET              ((u32)0x00002000)
#define SD_OCR_AKE_SEQ_ERROR            ((u32)0x00000008)
#define SD_OCR_ERRORBITS                ((u32)0xFDFFE008)

#define SD_R6_GENERAL_UNKNOWN_ERROR     ((u32)0x00002000)
#define SD_R6_ILLEGAL_CMD               ((u32)0x00004000)
#define SD_R6_COM_CRC_FAILED            ((u32)0x00008000)

#define SD_VOLTAGE_WINDOW_SD            ((u32)0x80100000)
#define SD_HIGH_CAPACITY                ((u32)0x40000000)
#define SD_STD_CAPACITY                 ((u32)0x00000000)
#define SD_CHECK_PATTERN                ((u32)0x000001AA)
#define SD_VOLTAGE_WINDOW_MMC           ((u32)0x80FF8000)

#define SD_MAX_VOLT_TRIAL               ((u32)0x0000FFFF)
#define SD_ALLZERO                      ((u32)0x00000000)

#define SD_WIDE_BUS_SUPPORT             ((u32)0x00040000)
#define SD_SINGLE_BUS_SUPPORT           ((u32)0x00010000)
#define SD_CARD_LOCKED                  ((u32)0x02000000)
#define SD_CARD_PROGRAMMING             ((u32)0x00000007)
#define SD_CARD_RECEIVING               ((u32)0x00000006)
#define SD_DATATIMEOUT                  ((u32)0xFFFFFFFF)
#define SD_0TO7BITS                     ((u32)0x000000FF)
#define SD_8TO15BITS                    ((u32)0x0000FF00)
#define SD_16TO23BITS                   ((u32)0x00FF0000)
#define SD_24TO31BITS                   ((u32)0xFF000000)
#define SD_MAX_DATA_LENGTH              ((u32)0x01FFFFFF)

#define SD_HALFFIFO                     ((u32)0x00000008)
#define SD_HALFFIFOBYTES                ((u32)0x00000020)

#define SD_CCCC_LOCK_UNLOCK             ((u32)0x00000080)
#define SD_CCCC_WRITE_PROT              ((u32)0x00000040)
#define SD_CCCC_ERASE                   ((u32)0x00000020)

/**
 * @brief SD_Init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_Init(void);
/**
 * @brief SDMMC_Clock_Set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param clkdiv 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void SDMMC_Clock_Set(u16 clkdiv);
/**
 * @brief SDMMC_Send_Cmd：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmdindex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param waitrsp 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param arg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void SDMMC_Send_Cmd(u8 cmdindex,u8 waitrsp,u32 arg);
/**
 * @brief SDMMC_Send_Data_Cfg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datatimeout 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param datalen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param blksize 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void SDMMC_Send_Data_Cfg(u32 datatimeout,u32 datalen,u8 blksize,u8 dir);
/**
 * @brief SD_PowerON：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_PowerON(void);
/**
 * @brief SD_PowerOFF：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_PowerOFF(void);
/**
 * @brief SD_InitializeCards：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_InitializeCards(void);
/**
 * @brief SD_GetCardInfo：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cardinfo 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_GetCardInfo(SD_CardInfo *cardinfo);
/**
 * @brief SD_EnableWideBusOperation：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param wmode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_EnableWideBusOperation(u32 wmode);
/**
 * @brief SD_SelectDeselect：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_SelectDeselect(u32 addr);
/**
 * @brief SD_SendStatus：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pcardstatus 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_SendStatus(uint32_t *pcardstatus);
/**
 * @brief SD_GetState：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SDCardState SD_GetState(void);
/**
 * @brief SD_ReadBlocks：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param blksize 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param nblks 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_ReadBlocks(u8 *buf,long long  addr,u16 blksize,u32 nblks);
/**
 * @brief SD_WriteBlocks：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param blksize 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param nblks 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SD_WriteBlocks(u8 *buf,long long addr,u16 blksize,u32 nblks);
/**
 * @brief CmdError：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdError(void);
/**
 * @brief CmdResp7Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp7Error(void);
/**
 * @brief CmdResp1Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp1Error(u8 cmd);
/**
 * @brief CmdResp3Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp3Error(void);
/**
 * @brief CmdResp2Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp2Error(void);
/**
 * @brief CmdResp6Error：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param prca 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error CmdResp6Error(u8 cmd,u16*prca);
/**
 * @brief SDEnWideBus：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param enx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error SDEnWideBus(u8 enx);
/**
 * @brief IsCardProgramming：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pstatus 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error IsCardProgramming(u8 *pstatus);
/**
 * @brief FindSCR：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param rca 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pscr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
SD_Error FindSCR(u16 rca,u32 *pscr);

/**
 * @brief SD_ReadDisk：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sector 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cnt 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
u8 SD_ReadDisk(u8*buf,u32 sector,u32 cnt);
/**
 * @brief SD_WriteDisk：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sector 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cnt 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
u8 SD_WriteDisk(u8*buf,u32 sector,u32 cnt);

#endif
