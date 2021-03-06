#include "task_peripheral_handling.h"
#include "ap_state_config.h"
#include "ap_browse.h"
#include "ap_display.h"

MSG_Q_ID PeripheralTaskQ;
void *peripheral_task_q_stack[PERIPHERAL_TASK_QUEUE_MAX];
static INT8U peripheral_para[PERIPHERAL_TASK_QUEUE_MAX_MSG_LEN];
INT8U screen_saver_enable = 0;
INT32U battery_charge_icon_blink_cnt = 0;
INT32U battery_low_blink_cnt = 0;
INT32U display_insert_sdc_cnt = 0;
INT32U motion_detect_peripheral_cnt = 0;
INT32U G_sensor_power_on_time = 0;
INT32U sensor_error_power_off_timer = 0;

#if TV_DET_ENABLE
extern INT8U tv_debounce_cnt;
#endif

extern INT8U PreviewIsStopped;
extern volatile INT8U cdsp_hangup_flag;
extern INT8U adp_status;
extern INT32U gp_ae_awb_process(void);
extern void gp_isp_iso_set(INT32U iso);
extern void set_led_mode(LED_MODE_ENUM mode);
static INT8U watch_dog_cnt=0;

void task_peripheral_handling_init(void)
{
	PeripheralTaskQ = msgQCreate(PERIPHERAL_TASK_QUEUE_MAX, PERIPHERAL_TASK_QUEUE_MAX, PERIPHERAL_TASK_QUEUE_MAX_MSG_LEN);
	ap_peripheral_init();
}
INT8U card_space_less_flag=0;

void task_peripheral_handling_entry(void *para)
{
	INT32U msg_id, power_on_auto_rec_delay_cnt;
	INT8U usb_detect_start;
   #if TV_DET_ENABLE   // for tv out
	INT8U tick = 0, tv_polling_start = 0;
   #endif   // end for tv out
	INT8U aeawb_start;
	INT8S ret = 0;
	INT8U volume_show_time;

	aeawb_start = 1;
	volume_show_time = 0;
	usb_detect_start = 0;
	while(1) {
		if(msgQReceive(PeripheralTaskQ, &msg_id, peripheral_para, PERIPHERAL_TASK_QUEUE_MAX_MSG_LEN) == STATUS_FAIL) {
			continue;
		}
        switch (msg_id) {
        	case MSG_PERIPHERAL_TASK_KEY_REGISTER:
        		ap_peripheral_key_register(peripheral_para[0]);
        		break;

        	case MSG_PERIPHERAL_TASK_USBD_DETECT_INIT:
        		usb_detect_start = 1;
				power_on_auto_rec_delay_cnt = 2*128/PERI_TIME_INTERVAL_AD_DETECT;	//2s				
        		break;

        	case MSG_PERIPHERAL_TASK_LED_SET:
        		//ap_peripheral_led_set(peripheral_para[0]);
        		set_led_mode(peripheral_para[0]);
				if(peripheral_para[0] ==LED_CARD_NO_SPACE)
					card_space_less_flag =1;
        		break;
        	case MSG_PERIPHERAL_TASK_LED_FLASH_SET:
				//ap_peripheral_led_flash_set();
        		break;
			case MSG_PERIPHERAL_TASK_LED_BLINK_SET:
				//ap_peripheral_led_blink_set();
				break;
			
			case MSG_PERIPHERAL_TASK_BATTERY_CHARGE_ICON_BLINK_START:
				battery_charge_icon_blink_cnt = 0x002f;
				break;
			case MSG_PERIPHERAL_TASK_BATTERY_CHARGE_ICON_BLINK_STOP:
				battery_charge_icon_blink_cnt = 0;
				break;
				
			case MSG_PERIPHERAL_TASK_BATTERY_LOW_BLINK_START:
				battery_low_blink_cnt = 0x001f;
				battery_low_blink_cnt |= 0x0a00;
				break;
			case MSG_PERIPHERAL_TASK_BATTERY_LOW_BLINK_STOP:
				if(battery_low_blink_cnt){
					battery_low_blink_cnt=0;
				}
				break;
			case MSG_PERIPHERAL_TASK_DISPLAY_PLEASE_INSERT_SDC:
				display_insert_sdc_cnt = 0x006f;
				break;
			case MSG_PERIPHERAL_TASK_TIMER_VOLUME_ICON_SHOW:
				volume_show_time = peripheral_para[0]*20;
				break;
			
			case MSG_PERIPHERAL_TASK_G_SENSOR_POWER_ON_START:
				G_sensor_power_on_time = 0x0300;
				break;
			case MSG_PERIPHERAL_TASK_TIME_SET:
				
				ap_peripheral_time_set();
				break;

#if USE_ADKEY_NO
        	case MSG_PERIPHERAL_TASK_AD_DETECT_CHECK:
        		ap_peripheral_key_judge();
        		//ap_peripheral_ad_key_judge();
        		
        		#if GPDV_BOARD_VERSION != GPCV1237A_Aerial_Photo
        		ap_peripheral_config_store();
        		#endif

        		if(usb_detect_start == 1) {
        			ap_peripheral_adaptor_out_judge();
        			#if GPDV_BOARD_VERSION != GPCV1237A_Aerial_Photo
					ap_peripheral_hdmi_detect();
					#endif
        			

					if(cdsp_hangup_flag) { //10s
						PreviewIsStopped = 1;
						DBG_PRINT("too many cdsp overflow!!! Auto Power Off...\r\n");
						msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_URGENT);
					} else if(PreviewIsStopped == 0) {					     
						if(sensor_error_power_off_timer > 10*128/PERI_TIME_INTERVAL_AD_DETECT) { //10s auto power off
							PreviewIsStopped = 1;
							DBG_PRINT("no scaler interrup occurs!!! Auto Power Off...\r\n");
							msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_URGENT);
						}
						else
						{   
						    sensor_error_power_off_timer++;
						}
					}
        		}
				
				
           
				if(aeawb_start) {			
					ret = gp_ae_awb_process();
					if(ret != 0) {
						//DBG_PRINT(".");
					}
					else {
						//DBG_PRINT("\r\nae, awb process done!! \r\n");
						//DBG_PRINT("*");
					}
				}	
        		
				break;
#endif

         #if TV_DET_ENABLE   // for tv out
            case MSG_PERIPHERAL_TV_POLLING_START:
                tv_polling_start = 1;
			#if TV_DET_ENABLE
               	tv_debounce_cnt = 0;
            #endif
                break;

            case MSG_PERIPHERAL_TV_POLLING_STOP:
                tv_polling_start = 0;
			#if TV_DET_ENABLE
               	tv_debounce_cnt = 0;
            #endif
                break;

			case MSG_PERIPHERAL_TASK_TV_DETECT:
				ap_peripheral_tv_detect();
				break;
         #endif   // end for tv out
		 
#if GPDV_BOARD_VERSION != GPCV1237A_Aerial_Photo
			case MSG_PERIPHERAL_TASK_READ_GSENSOR:
				ap_peripheral_read_gsensor();
				break;
#endif

#if C_MOTION_DETECTION == CUSTOM_ON
//        	case MSG_PERIPHERAL_TASK_MOTION_DETECT_JUDGE:
//        		ap_peripheral_motion_detect_judge();
//        		break;
        	case MSG_PERIPHERAL_TASK_MOTION_DETECT_START:
        		ap_peripheral_motion_detect_start();
        		motion_detect_peripheral_cnt= 32;
        		break;
        	case MSG_PERIPHERAL_TASK_MOTION_DETECT_STOP:
        		ap_peripheral_motion_detect_stop();
        		motion_detect_peripheral_cnt= 0;
        		break;
        	case MSG_PERIPHERAL_TASK_MOTION_DETECT_DELAY:
        		motion_detect_peripheral_cnt= 0xff;
        		break;
#endif
#if C_SCREEN_SAVER == CUSTOM_ON
			case MSG_PERIPHERAL_TASK_LCD_BACKLIGHT_SET:
			ap_peripheral_clr_screen_saver_timer();
			ap_peripheral_lcd_backlight_set(peripheral_para[0]);
        		break;
			case MSG_PERIPHERAL_TASK_SCREEN_SAVER_ENABLE:
        		screen_saver_enable = 1;
        		break;
#endif
			case MSG_PERIPHERAL_TASK_NIGHT_MODE_SET:
        		//ap_peripheral_night_mode_set(peripheral_para[0]);
				break;

        	case MSG_PERIPHERAL_USBD_EXIT:
        		usbd_exit = 1;
        		break;

			case MSG_PERIPHERAL_TASK_ISP_ISO_SET:
				gp_isp_iso_set(peripheral_para[0]);
				break;

	       	default:
	       		break;
        }
    }
}
