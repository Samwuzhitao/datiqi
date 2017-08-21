/**
  ******************************************************************************
  * @file   	rf_protocol.h
  * @author  	Samwu
  * @version 	V1.0.0.0
  * @date   	2017.08.14
  * @brief   	board infomation
  ******************************************************************************
  */
#ifndef _RF_PROTOCOL_V0200_H_
#define _RF_PROTOCOL_V0200_H_
#include "stm32f10x.h"

//#define ENABLE_SPI_DECODE_DEBUG

#ifdef ENABLE_SPI_DECODE_DEBUG
#define SPI_DECODE_DEBUG  b_print
#else
#define SPI_DECODE_DEBUG(...)
#endif

#define	NRF_TOTAL_DATA_LEN				    (250)				// 2.4G数据总长度
#define RF_REV_DATA_LEN               (16)        // 预留扩展的最大数据长度
#define VERSION_MAJOR                 (2)         // 协议主版本号
#define VERSION_MINOR                 (0)         // 协议次版本号
#define RF_DEV_ID_LEN                 (4)         // 设备ID的长度

#define RF_PACK_SOF                   (0x61)
#define RF_PACK_EOF                   (0x21)

typedef enum {
	DEVICE_JSQ = 0x01, // 0x01:STM32
	DEVICE_DTQ = 0x11, // 0x11:答题器
	DEVICE_JSD = 0x12  // 0x12:教师端
}dtq_dev_t;

typedef union {
	uint8_t byte;
	struct
	{
		uint8_t major:4;   /*!< bit:  0..3  MAJOR VERSION */
		uint8_t minor:4;   /*!< bit:  4..7  MINOR VERSION */
	}bits;
}ver_num_t;

typedef struct
{
	uint8_t	 	src_uid[RF_DEV_ID_LEN];
	uint8_t		dst_uid[RF_DEV_ID_LEN];
	dtq_dev_t dev_id;
	ver_num_t ver_num;
	uint8_t   seq_num;
	uint8_t   pac_num;
}rf_pack_ctl_t;

typedef struct
{
	uint8_t         head;
	rf_pack_ctl_t   ctl;
	uint8_t         rev_len;
	uint8_t         rev_data[16];
	uint8_t					pack_len;
	uint8_t					data[NRF_TOTAL_DATA_LEN];
	uint8_t         crc_xor;
	uint8_t         end;
}rf_pack_t;

void rf_pro_init_pack( rf_pack_t *rf_pack );
void rf_pro_pack_update_crc( rf_pack_t *rf_pack );
void rf_pro_pack_debug( rf_pack_t *rf_pack );
void rf_pro_pack_num_add( rf_pack_t *rf_pack );

#endif
