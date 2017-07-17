/**
  ******************************************************************************
  * @file   	app_card_process.c
  * @author  	sam.wu
  * @version 	V1.0.0.0
  * @date   	2015.11.05
  * @brief
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_serial_cmd_process.h"
#include "app_card_process.h"
#include "app_show_message_process.h"

//#define SHOW_CARD_PROCESS_TIME
extern uint8_t g_cSNR[10];	
extern wl_typedef        wl;
extern revicer_typedef   revicer;
extern rf_config_typedef clicker_set;
task_tcb_typedef card_task;

#ifdef SHOW_CARD_PROCESS_TIME
extern __IO uint32_t     PowerOnTime;
uint32_t StartTime,EndTime;
#endif

static uint8_t card_process_status = 0;
static uint8_t is_white_list_uid = 0;
/* 返回卡类型 */
uint8_t g_cardType[40];	
uint8_t respon[BUF_LEN + 20];	
static rf_id_typedf rID,wID;

uint16_t NDEF_Len               = 0;
static uint8_t g_uid_len        = 0;
static uint8_t wtrte_flash_ok   = 0;
static uint16_t read_uid_pos    = 0xFFFF;
static uint16_t write_uid_pos   = 0xFFFF;
static uint8_t card_message_err = 0;
static uint8_t find_card_ok     = 0;

/******************************************************************************
  Function:rf_set_card_status
  Description:
		修改systick的状态
  Input :
		rf_status: systick的新状态
  Output:
  Return:
  Others:None
******************************************************************************/
void rf_set_card_status(uint8_t new_status)
{
	card_process_status = new_status;
	//printf("rf_set_card_status  = %d\r\n",card_process_status);
}

/******************************************************************************
  Function:rf_get_card_status
  Description:
		获取systick的状态
  Input :
  Output:systick的新状态
  Return:
  Others:None
******************************************************************************/
uint8_t rf_get_card_status(void)
{
	return card_process_status ;
}

/******************************************************************************
  Function:App_card_process
  Description:
		App MI Card 轮询处理函数
  Input :
  Return:
  Others:None
******************************************************************************/
void App_card_process(void)
{
	/* 获取当前状态 */
	uint8_t card_current_status = 0;
	
	card_current_status = rf_get_card_status();

	if( card_current_status == 1 )
	{
		uint8_t status = 0;
		#ifdef SHOW_CARD_PROCESS_TIME
		StartTime = PowerOnTime;
		#endif

		PcdAntennaOn();
	  //MRC500_DEBUG_START("PcdRequest \r\n");
		memset(g_cardType, 0, 40);
		/* reqA指令 :请求A卡，返回卡类型，不同类型卡对应不同的UID长度 */
		status = PcdRequest(PICC_REQIDL,g_cardType);
	  //MRC500_DEBUG_END();
		if( status == MI_OK )
		{
			if(find_card_ok == 1)
			{
				sw_clear_timer(&card_second_find_timer);
				return;
			}
			if(find_card_ok == 2)
			{
				find_card_ok = 0;
			}
		
			if( (g_cardType[0] & 0x40) == 0x40)
			{	
				g_uid_len = 8;	
			}
			else
			{	
				g_uid_len = 4;
			}
			DEBUG_CARD_DEBUG_LOG("uid len = %d\r\n",g_uid_len);
		}
		else
		{
			return;
		}
		/* 防碰撞1 */
		MRC500_DEBUG_START("PcdAnticoll \r\n");
		status = PcdAnticoll(PICC_ANTICOLL1, g_cSNR);
		MRC500_DEBUG_END();
		DEBUG_CARD_DEBUG_LOG("PcdSelect1 status = %d respon[0] = %02x respon[1] = %02x\r\n",
		status , respon[0], respon[1] );
		if( status != MI_OK )
		{
			return;
		}

		/* 选卡1 */
		memset(respon, 0, 10);
		status = PcdSelect1(g_cSNR, respon);
		//DEBUG_CARD_DEBUG_LOG("PcdSelect1 status = %d respon[0] = %02x respon[1] = %02x\r\n",
		//status , respon[0], respon[1] );
		if( status == MI_OK )
		{
			if((g_uid_len == 8) && ((respon[0] & 0x04) == 0x04))
			{
				DEBUG_CARD_DEBUG_LOG("PcdSelect1 status = %d\r\n",status);
				//MRC500_DEBUG_START("PICC_ANTICOLL2 \r\n");
				status = PcdAnticoll(PICC_ANTICOLL2, &g_cSNR[4]);
				//MRC500_DEBUG_END();
				if( status != MI_OK)
				{
					return;
				}
				status = PcdSelect2(&g_cSNR[4], respon);
				if( status == MI_OK)
				{
					rf_set_card_status(2);
					DEBUG_CARD_DEBUG_LOG("PcdSelect2 status = %d\r\n",status);
				}
				else
				{
					return;
				}
			}
			if(g_uid_len == 4)
			{
				wtrte_flash_ok = 1;
				rf_set_card_status(3);
				card_message_err = 1;
			}
		}
		else
		{
			return;
		}
		#ifdef SHOW_CARD_PROCESS_TIME
		EndTime = PowerOnTime - StartTime;
		printf("UseTime:PcdSelect2 = %d \r\n",EndTime);
		#endif
	}

	if( card_current_status == 2 )
	{
		uint8_t status = 0;
		/*选择应用*/
		status = SelectApplication();
		DEBUG_CARD_DEBUG_LOG("SelectApplication status = %d\r\n",status);
		if( status != MI_OK )
		{
			mfrc500_init();
			rf_set_card_status(1);
			return;
		}
		#ifdef SHOW_CARD_PROCESS_TIME
		EndTime = PowerOnTime - StartTime;
		printf("UseTime:SelectApplication = %d \r\n",EndTime);
		#endif
		/* 考勤指令 */
		if( wl.attendance_sttaus == ON )
		{
			uint8_t *pdata = (uint8_t *)&rID;
			uint8_t card_data_len = sizeof(rf_id_typedf);
			status = ReadNDEFfile(pdata, &NDEF_Len);
			DEBUG_CARD_DEBUG_LOG("ReadNDEFfile status = %d\r\n",status);
			if( status != MI_OK )
			{
				memset(pdata,00,card_data_len);
				mfrc500_init();
				rf_set_card_status(1);
				return;
			}
			else
			{
				if(rID.data_xor != XOR_Cal(rID.uid,card_data_len-3))
				{
					uint8_t i;
					DEBUG_CARD_DATA_LOG("NDEF_DataRead :");
					pdata = rID.uid;
					for(i=0;i<card_data_len-3;i++)
						DEBUG_CARD_DATA_LOG("%02x ",pdata[i]);
					DEBUG_CARD_DATA_LOG("\r\n");
					memset(pdata,00,card_data_len);
					DEBUG_CARD_DATA_LOG("NDEF_DataRead XOR ERROR!\r\n");
					mfrc500_init();
					rf_set_card_status(1);
					return;
				}
				else
				{
					uint8_t i;
					DEBUG_CARD_DATA_LOG("NDEF_DataRead :");
					for(i=0;i<card_data_len;i++)
						DEBUG_CARD_DATA_LOG("%02x ",pdata[i]);
					DEBUG_CARD_DATA_LOG("\r\n");
					DEBUG_CARD_DATA_LOG("NDEF_DataRead Clear!\r\n");
				}
			}

			is_white_list_uid = search_uid_in_white_list(g_cSNR+4,&read_uid_pos);
			if(is_white_list_uid == OPERATION_ERR)
			{
				read_uid_pos = 0xFFFF;
			}
			card_message_err = 1;
			wtrte_flash_ok = 1;
			#ifdef SHOW_CARD_PROCESS_TIME
			EndTime = PowerOnTime - StartTime;
			printf("UseTime:ReadNDEFfile = %d \r\n",EndTime);
			#endif
			rf_set_card_status(3);
		}

		/* 配对指令 */
		if( wl.match_status == ON )
		{
			uint8_t *rpdata = (uint8_t *)&rID;
			uint8_t *wpdata = (uint8_t *)&wID;
			is_white_list_uid = add_uid_to_white_list(g_cSNR+4,&write_uid_pos);

			if(is_white_list_uid != OPERATION_ERR)
			{
				uint8_t card_data_len = sizeof(rf_id_typedf);
				card_message_err  = 1;
				wID.len_h  = 0;
				wID.len_l = card_data_len;
				memcpy(wID.uid,revicer.uid,4);
				wID.upos  = write_uid_pos;
				memcpy(&(wID.rf_conf),&clicker_set,sizeof(rf_config_typedef));

				/* 重新写入数据检测 */
				status = ReadNDEFfile(rpdata, &NDEF_Len);
				DEBUG_CARD_DEBUG_LOG("ReadNDEFfile0 status = %d\r\n",status);
				#ifdef SHOW_CARD_PROCESS_TIME
				EndTime = PowerOnTime - StartTime;
				printf("UseTime:ReadNDEFfile0 = %d \r\n",EndTime);
				#endif
				if( status != MI_OK )
				{
					memset(rpdata,0x00,card_data_len);
					memset(wpdata,0x00,card_data_len);
					mfrc500_init();
					rf_set_card_status(1);
					return;
				}
				else
				{
					uint8_t i;
					uint8_t write_flg = 0;
					DEBUG_CARD_DATA_LOG("NDEF_DataRead :");
					for(i=0;i<card_data_len;i++)
						DEBUG_CARD_DATA_LOG("%02x ",rpdata[i]);
					DEBUG_CARD_DATA_LOG("\r\n");

					memcpy( wID.stdid, rID.stdid, 10 );
					if(wl.weite_std_id_status == ON)
					{
						memcpy(wID.stdid,card_task.stdid,10);
					}
					wID.data_xor = XOR_Cal(wID.uid,card_data_len-3);

					/* 完全比较，否则存在异或校验的巧合*/
				  for(i=0;i<card_data_len;i++)
					{
						if(rpdata[i] != wpdata[i])
							write_flg = 1;
					}

					if(write_flg != 1)
					{
						/* 无需写入新数据,直接进入下一流程 */
						card_message_err = 1;
						wtrte_flash_ok = 1;
						rf_set_card_status(3);
						return;
					}
				}

				#ifdef SHOW_CARD_PROCESS_TIME
				EndTime = PowerOnTime - StartTime;
				printf("UseTime:WriteNDEFfile0 = %d \r\n",EndTime);
				#endif
				status = WriteNDEFfile(wpdata);
				DEBUG_CARD_DEBUG_LOG("WriteNDEFfile1 status = %d\r\n",status);
				#ifdef SHOW_CARD_PROCESS_TIME
				EndTime = PowerOnTime - StartTime;
				printf("UseTime:WriteNDEFfile1 = %d \r\n",EndTime);
				#endif
				if( status != MI_OK )
				{
					DEBUG_CARD_DATA_LOG("NDEF_DataWrite: ERR");
					memset(wpdata,0x00,card_data_len);
					mfrc500_init();
					rf_set_card_status(1);
					return;
				}
				else
				{
					uint8_t i;
					DEBUG_CARD_DATA_LOG("NDEF_DataWrite:");
					for(i=0;i<card_data_len;i++)
						DEBUG_CARD_DATA_LOG("%02x ",wpdata[i]);
					DEBUG_CARD_DATA_LOG("\r\n");
				}
				status = ReadNDEFfile(rpdata, &NDEF_Len);
				DEBUG_CARD_DEBUG_LOG("ReadNDEFfile status = %d\r\n",status);
				#ifdef SHOW_CARD_PROCESS_TIME
				EndTime = PowerOnTime - StartTime;
				printf("UseTime:ReadNDEFfile = %d \r\n",EndTime);
				#endif
				if( status != MI_OK )
				{
					memset(rpdata,0x00,card_data_len);
					memset(wpdata,0x00,card_data_len);
					mfrc500_init();
					rf_set_card_status(1);
					return;
				}
				else
				{
					uint8_t i;
					uint8_t write_flg = 0;
					DEBUG_CARD_DATA_LOG("NDEF_DataRead :");
					for(i=0;i<card_data_len;i++)
						DEBUG_CARD_DATA_LOG("%02x ",rpdata[i]);
					if(rID.data_xor == XOR_Cal(rID.uid,card_data_len-3))
							DEBUG_CARD_DATA_LOG(" XOR OK!");
					DEBUG_CARD_DATA_LOG("\r\n");
					
					for(i=0;i<card_data_len;i++)
					{
						if(rpdata[i] != wpdata[i])
							write_flg = 1;
					}

					if(write_flg == 1)
					{
						memset(rpdata,0x00,card_data_len);
						memset(wpdata,0x00,card_data_len);
						mfrc500_init();
						rf_set_card_status(1);
						return;
					}
				}
				if( status != MI_OK )
				{
					memset(rpdata,0x00,card_data_len);
					memset(wpdata,0x00,card_data_len);
					mfrc500_init();
					rf_set_card_status(1);
					return;
				}
				wtrte_flash_ok = 1;
			}
			rf_set_card_status(3);
		}
		
		if( wl.attendance_sttaus == ON )
		{
			uint8_t *rpdata = (uint8_t *)&rID;
			uint8_t card_data_len = sizeof(rf_id_typedf);

			status = ReadNDEFfile(rpdata, &NDEF_Len);
			if( status != MI_OK )
			{
				memset(rpdata,0x00,card_data_len);
				mfrc500_init();
				rf_set_card_status(1);
				return;
			}
			else
			{
				if(rID.data_xor != XOR_Cal(rID.uid,card_data_len-3))
				{
					memset(rpdata,0x00,card_data_len);
					mfrc500_init();
					rf_set_card_status(1);
				}
			}

			is_white_list_uid = search_uid_in_white_list(g_cSNR+4,&read_uid_pos);
			if(is_white_list_uid == OPERATION_ERR)
			{
				read_uid_pos = 0xFFFF;
			}
			card_message_err = 1;
			wtrte_flash_ok = 1;
			rf_set_card_status(3);
		}
	}

	if( card_current_status == 3 )
	{
		if(wtrte_flash_ok == 1)
		{
			if( card_message_err != 2 )
			{
				if( g_uid_len == 8 )
				{
					uint8_t status;
					status = SendInterrupt();
					DEBUG_CARD_DEBUG_LOG("SendInterrupt status = %d\r\n",status);
					#ifdef SHOW_CARD_PROCESS_TIME
					EndTime = PowerOnTime - StartTime;
					printf("UseTime:SendInterrupt = %d \r\n",EndTime);
					#endif
					if( status != MI_OK )
					{
						uint8_t *rpdata = (uint8_t *)&rID;
						uint8_t *wpdata = (uint8_t *)&wID;
						uint8_t card_data_len = sizeof(rf_id_typedf);
						memset(rpdata,0x00,card_data_len);
						memset(wpdata,0x00,card_data_len);
						mfrc500_init();
						rf_set_card_status(1);
						return;
					}
				}
				#ifdef OPEN_SILENT_MODE
				ledOn(LBLUE);
				#else
				BEEP_EN();
				#endif
				Deselect();
				PcdHalt();
				PcdAntennaOff();
			}
		}

		if( wl.match_status == ON )
		{
			if(wl.weite_std_id_status == ON)
			{
				char str[21];
				b_print("{\r\n");
				b_print("  \"fun\": \"update_student_id_info\",\r\n");
				memset(str,0,20);
				sprintf(str, "%010u" , *(uint32_t *)( wl.uids[write_uid_pos].uid));
				b_print("  \"card_id\": \"%s\",\r\n", str );
				if( wl.is_printf_clear_uid == 1 )
				{
					wl.is_printf_clear_uid = 0;
					memset(str,0,20);
					sprintf(str, "%010u" , *(uint32_t *)( wl.clear_uid));
					b_print("  \"replace_uid\": \"%d\",\r\n",str);
				}
				else
				{
					b_print("  \"replace_uid\": \"\",\r\n");
				}

				memset(str,0,21);
				{
					uint8_t i, temp_data;
					char *p_student_data = str;

					for(i=0;(i<10) && ((rID.stdid[i]&0xF0) != 0xF0);i++)
					{
						sprintf(p_student_data+2*i,  "%d" , (rID.stdid[i]&0xF0)>>4);
						temp_data = rID.stdid[i]&0x0F;
						if(temp_data != 0x0F)
							sprintf(p_student_data+2*i+1,"%d" ,temp_data);
					}
				}
				b_print("  \"student_id\": \"%s\"\r\n", str );
				b_print("}\r\n");
			}
			else
			{

			}
		}
		if( wl.attendance_sttaus == ON )
		{

		}

		if(card_message_err != 0)
		{
			if(( wtrte_flash_ok == 1 ) && (wl.weite_std_id_status == OFF))
			{
				#ifndef OPEN_CARD_DATA_SHOW 
				char str[20];
//  		uint8_t i,uid[4];
				b_print("{\r\n");
				b_print("  \"fun\": \"update_card_info\",\r\n");
				memset(str,0,20);
//			b_print("  \"HEX\": \"%02X%02X%02X%02X\",\r\n",
//			wl.uids[write_uid_pos].uid[0],wl.uids[write_uid_pos].uid[1],
//			wl.uids[write_uid_pos].uid[2],wl.uids[write_uid_pos].uid[3]);
				if( g_uid_len == 8 )
				{
					b_print("  \"card_id\": \"%010u\",\r\n",*(uint32_t *)( wl.uids[write_uid_pos].uid));
					if( wl.is_printf_clear_uid == 1 )
					{
						wl.is_printf_clear_uid = 0;
						b_print("  \"replace_uid\": \"%010u\"\r\n",*(uint32_t *)( wl.clear_uid));
						memset(wl.clear_uid,0x00,4);
					}
					else
					{
						b_print("  \"replace_uid\": \"\"\r\n");
					}
				}
				if( g_uid_len == 4 )
				{
					b_print("  \"card_type\": \"M1\",\r\n");
					b_print("  \"card_id\": \"%010u\",\r\n",*(uint32_t *)( g_cSNR ));
					b_print("  \"replace_uid\": \"\"\r\n");
				}
//			memset(str,0,20);
//			for(i=0;i<4;i++)
//				uid[i] = wl.uids[write_uid_pos].uid[3-i];
//			sprintf(str, "%010u" , *(uint32_t *)( uid ));
//			b_print("  \"正码\": \"%s\"\r\n", str );
				b_print("}\r\n");
				DEBUG_CARD_DATA_LOG("NDEF_DataRead and NDEF_DataWrite Clear!\r\n");
				#endif
			}
		}
		rf_set_card_status(4);
	}

	if( card_current_status == 5 )
	{
		uint8_t *rpdata = (uint8_t *)&rID;
		uint8_t *wpdata = (uint8_t *)&wID;
		#ifdef OPEN_SILENT_MODE
		ledOff(LBLUE);
		#else
		BEEP_DISEN();
		#endif
		rf_set_card_status(1);
		memset(rpdata,0x00,sizeof(rf_id_typedf));
		memset(wpdata,0x00,sizeof(rf_id_typedf));
		if( wl.weite_std_id_status == ON )
		{
			memset(wID.stdid,0x00,10);
			wl.weite_std_id_status = OFF;
			wl.match_status = OFF;
			rf_set_card_status(0);
		}
		find_card_ok = 1;
		#ifdef SHOW_CARD_PROCESS_TIME
		EndTime = PowerOnTime - StartTime;
		printf("UseTime:SecondFindStart = %d \r\n",EndTime);
		#endif
	}
}

/******************************************************************************
  Function:systick_timer_init
  Description:
  Input :
  Return:
  Others:None
******************************************************************************/
void card_timer_init( void )
{
	sw_create_timer(&card_buzzer_timer    , 150, 4, 5,&(card_process_status), NULL);
	sw_create_timer(&card_second_find_timer,300, 1, 2,&(find_card_ok), NULL);
}
