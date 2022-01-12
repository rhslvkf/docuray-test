#include "stcommon.h"
#include "stcommon_noninit.h"
#include "stcommon_model.h"
#include "st_gpio.h"
#include "st_eint.h"
#include "st_util.h"
#include "st_sleep.h"
#include "stio_gpio.h"
#include "stio_timer.h"
#include "stio_ctrl.h"

static void _stio_gpio_input1_hisr(void);
static void _stio_gpio_input2_hisr(void);
static void _stio_gpio_input3_hisr(void);
static void _stio_gpio_input4_hisr(void);
static void _stio_gpio_input5_hisr(void);
static void _stio_gpio_change_input_status(uint8 port_num, bool port_status);


static void _stio_gpio_init_gpsled(void);
static void _stio_gpio_init_gprsled(void);

static void _stio_gpio_init_ignition(void);

static void _stio_gpio_init_input1(void);
static void _stio_gpio_init_input2(void);
static void _stio_gpio_init_input3(void);
static void _stio_gpio_init_input4(void);
static void _stio_gpio_init_input5(void);

static void _stio_gpio_init_output1(void);
static void _stio_gpio_init_output2(void);
static void _stio_gpio_init_output3(void);

static void _stio_gpio_set_output1(bool status, bool active, bool lock);
static void _stio_gpio_set_output2(bool status, bool active, bool lock);
static void _stio_gpio_set_output3(bool status, bool active, bool lock);

static void _stio_gpio_set_in_state(uint8 no, bool state);
static void _stio_gpio_set_out_state(uint8 no, bool state, bool active, bool lock);
static void _stio_gpio_check_sw_reset(void);


#define HW_DEB_TIME		10

static const INPUT_PORT_STRUCT st4300_input_gpio[]=
{
	{IGNITION_GPIO_PORT, IGNITION_EINT_NO, IGNITION_EINT_ALT},
	{INPUT1_GPIO_PORT, INPUT1_EINT_NO, INPUT1_EINT_ALT},
	{NULL, NULL, NULL},
	{INPUT3_GPIO_PORT, INPUT3_EINT_NO, INPUT3_EINT_ALT},
	{INPUT4_GPIO_PORT, INPUT4_EINT_NO, INPUT4_EINT_ALT},
	{NULL, NULL, NULL},
};

static const INPUT_PORT_STRUCT st4340_input_gpio[]=
{
	{IGNITION_GPIO_PORT, IGNITION_EINT_NO, IGNITION_EINT_ALT},
	{INPUT1_GPIO_PORT, INPUT1_EINT_NO, INPUT1_EINT_ALT},
	{INPUT2_GPIO_PORT, INPUT2_EINT_NO, INPUT2_EINT_ALT},
	{INPUT3_GPIO_PORT, INPUT3_EINT_NO, INPUT3_EINT_ALT},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
};

static const INPUT_PORT_STRUCT st3300R_input_gpio[]=
{
	{IGNITION_GPIO_PORT, IGNITION_EINT_NO, IGNITION_EINT_ALT},
	{INPUT1_GPIO_PORT, INPUT1_EINT_NO, INPUT1_EINT_ALT},
	{NULL, NULL, NULL},
	{INPUT3_GPIO_PORT, INPUT3_EINT_NO, INPUT3_EINT_ALT},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
};


// jwpark_4500
static const INPUT_PORT_STRUCT st4500_input_gpio[]=
{
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
};

// <<<<< GIT
static const INPUT_PORT_STRUCT st4340H_input_gpio[]=
{
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
    {NULL, NULL, NULL},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
};	


static const INPUT_PORT_STRUCT st4310_input_gpio[]=	// 4310, 4310U, 4310W, 4310R, 4310S, 4310M
{
	{IGNITION_GPIO_PORT, IGNITION_EINT_NO, IGNITION_EINT_ALT},
	{INPUT1_GPIO_PORT, INPUT1_EINT_NO,	INPUT1_EINT_ALT},
	{INPUT2_GPIO_PORT, INPUT2_EINT_NO,	INPUT2_EINT_ALT},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
	{NULL, NULL, NULL},
};
// GIT >>>>>

static const uint8 st4300_output_gpio[]=
{
	OUTPUT1_GPIO_PORT,
	OUTPUT2_GPIO_PORT,
	OUTPUT3_GPIO_PORT
};

static const uint8 st4340_output_gpio[]=
{
	OUTPUT1_GPIO_PORT,
	OUTPUT2_GPIO_PORT,
};

static const uint8 st3300R_output_gpio[]=
{
	OUTPUT1_GPIO_PORT,
	OUTPUT2_GPIO_PORT,
};

// jwpark_4500
static const uint8 st4500_output_gpio[]=
{
	NULL
};

// <<<<< GIT
static const uint8 st4340H_output_gpio[]=
{
	OUTPUT1_GPIO_PORT,
	NULL,
	NULL
};
// GIT >>>>>

static const uint8 st4310_output_gpio[]= // 4310, 4310U, 4310W, 4310R, 4310S, 4310M
{
	OUTPUT1_GPIO_PORT,
	OUTPUT2_GPIO_PORT,
};

static const INPUT_PORT_STRUCT *input_port;
static const uint8 *output_port;

static bool	eint_polarity[EVENT_IN_NO_MAX] = {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE};
static uint8 in_state = 0, out_state = 0;

static bool	sw_reset = FALSE, sw_reset_check[3] = {FALSE, FALSE, FALSE};


static void _stio_gpio_change_input_status(uint8 port_num, bool port_status)
{
	INPUT_CHANGE_STR gpio_input_status_set;

	gpio_input_status_set.port = port_num;
	gpio_input_status_set.status = port_status;
	ST_SendMsg(MOD_STIO, MSG_ID_ST_IO_EINT_HISR, (void *)&gpio_input_status_set, sizeof(gpio_input_status_set));
}

static void _stio_gpio_ignition_hisr(void)
{
	_stio_gpio_change_input_status(EVENT_IGNITION, eint_polarity[EVENT_IGNITION]);
	//_stio_gpio_set_in_state(EVENT_IGNITION, eint_polarity[EVENT_IGNITION]);
	
	eint_polarity[EVENT_IGNITION] = stio_gpio_bool_algebra_reverse(eint_polarity[EVENT_IGNITION]);
	ST_EintSetup(input_port[0].eint_no, TRUE, eint_polarity[EVENT_IGNITION], HW_DEB_TIME, _stio_gpio_ignition_hisr);	
}

static void _stio_gpio_input1_hisr(void)
{
	_stio_gpio_change_input_status(EVENT_IN_NO_1, eint_polarity[EVENT_IN_NO_1]);
	_stio_gpio_set_in_state(EVENT_IN_NO_1, eint_polarity[EVENT_IN_NO_1]);
	
	eint_polarity[EVENT_IN_NO_1] = stio_gpio_bool_algebra_reverse(eint_polarity[EVENT_IN_NO_1]);
	ST_EintSetup(input_port[1].eint_no, TRUE, eint_polarity[EVENT_IN_NO_1], HW_DEB_TIME, _stio_gpio_input1_hisr);	
}


static void _stio_gpio_input2_hisr(void)
{
	_stio_gpio_change_input_status(EVENT_IN_NO_2, eint_polarity[EVENT_IN_NO_2]);
	_stio_gpio_set_in_state(EVENT_IN_NO_2, eint_polarity[EVENT_IN_NO_2]);
	
	eint_polarity[EVENT_IN_NO_2] = stio_gpio_bool_algebra_reverse(eint_polarity[EVENT_IN_NO_2]);
	ST_EintSetup(input_port[2].eint_no, TRUE, eint_polarity[EVENT_IN_NO_2], HW_DEB_TIME, _stio_gpio_input2_hisr);	
}

static void _stio_gpio_input3_hisr(void)
{
	_stio_gpio_change_input_status(EVENT_IN_NO_3, eint_polarity[EVENT_IN_NO_3]);
	_stio_gpio_set_in_state(EVENT_IN_NO_3, eint_polarity[EVENT_IN_NO_3]);
	
	eint_polarity[EVENT_IN_NO_3] = stio_gpio_bool_algebra_reverse(eint_polarity[EVENT_IN_NO_3]);
	ST_EintSetup(input_port[3].eint_no, TRUE, eint_polarity[EVENT_IN_NO_3], HW_DEB_TIME, _stio_gpio_input3_hisr);	
}

static void _stio_gpio_input4_hisr(void)
{
	_stio_gpio_change_input_status(EVENT_IN_NO_4, eint_polarity[EVENT_IN_NO_4]);
	_stio_gpio_set_in_state(EVENT_IN_NO_4, eint_polarity[EVENT_IN_NO_4]);
	
	eint_polarity[EVENT_IN_NO_4] = stio_gpio_bool_algebra_reverse(eint_polarity[EVENT_IN_NO_4]);
	ST_EintSetup(input_port[4].eint_no, TRUE, eint_polarity[EVENT_IN_NO_4], HW_DEB_TIME, _stio_gpio_input4_hisr);	
}

static void _stio_gpio_input5_hisr(void)
{
	_stio_gpio_change_input_status(EVENT_IN_NO_5, eint_polarity[EVENT_IN_NO_5]);
	_stio_gpio_set_in_state(EVENT_IN_NO_5, eint_polarity[EVENT_IN_NO_5]);
	
	eint_polarity[EVENT_IN_NO_5] = stio_gpio_bool_algebra_reverse(eint_polarity[EVENT_IN_NO_5]);
	ST_EintSetup(input_port[5].eint_no, TRUE, eint_polarity[EVENT_IN_NO_5], HW_DEB_TIME, _stio_gpio_input5_hisr);	
}


/***********************************
	LED & Out Status
	*TRUE : GPIO HIGH (LED ON)	
	*FALSE : GPIO LOW (LED OFF)
***********************************/
static void _stio_gpio_init_gpsled(void)	// GPS LED GPIO Init
{
	ST_GpioSetGpsledVoltage(TRUE);
	ST_GpioSetGpsled(FALSE);
}

static void _stio_gpio_init_gprsled(void)	//	GPRS LED GPIO Init
{
	ST_GpioSetkpledMode();
	ST_GpioSetkpled(FALSE);
}

typedef void (*input_hisr_fn)(void);
static const input_hisr_fn input_hisr_fn_tbl[EVENT_IN_NO_MAX]={
	_stio_gpio_ignition_hisr,
	_stio_gpio_input1_hisr,
	_stio_gpio_input2_hisr,
	_stio_gpio_input3_hisr,
	_stio_gpio_input4_hisr,
	_stio_gpio_input5_hisr
};

static void _gpio_init_input(EVENT_IN_NO n, const INPUT_PORT_STRUCT *p)
{
	ST_PRINT("IN %d INIT gpio %d, %d, %d", n, p->gpio_no, p->eint_no, p->alt_no);
	if(p->gpio_no==0)
	{
		ST_PRINT("IN %d  init err", n);
		return;
	}

	ST_GpioPullEn(p->gpio_no, TRUE);
	ST_GpioPullSel(p->gpio_no, TRUE);
	ST_GpioSetup(p->gpio_no, p->alt_no);
	
	if((ST_GpioIn(p->gpio_no) == 0 ) && (n != EVENT_IGNITION))
	{
		_stio_gpio_set_in_state(n, eint_polarity[n]);					
		eint_polarity[n] = TRUE;
	}
	
	ST_EintSetup(p->eint_no, TRUE, eint_polarity[n], HW_DEB_TIME, input_hisr_fn_tbl[n]);	
	ST_EintEnable(p->eint_no);
}

static void _stio_gpio_init_ignition(void)
{
	ST_PRINT("SSJ %s, %d, %d, %d", __FUNCTION__, input_port[EVENT_IGNITION].gpio_no, input_port[EVENT_IGNITION].eint_no, input_port[EVENT_IGNITION].alt_no);
	
	_gpio_init_input(EVENT_IGNITION, &input_port[EVENT_IGNITION]);
}

static void _stio_gpio_init_input1(void)
{		
	_gpio_init_input(EVENT_IN_NO_1, &input_port[EVENT_IN_NO_1]);
}

static void _stio_gpio_init_input2(void)
{
	_gpio_init_input(EVENT_IN_NO_2, &input_port[EVENT_IN_NO_2]);
}

static void _stio_gpio_init_input3(void)
{
	_gpio_init_input(EVENT_IN_NO_3, &input_port[EVENT_IN_NO_3]);
}

static void _stio_gpio_init_input4(void)
{
	_gpio_init_input(EVENT_IN_NO_4, &input_port[EVENT_IN_NO_4]);
}

static void _stio_gpio_init_input5(void)
{
	_gpio_init_input(EVENT_IN_NO_5, &input_port[EVENT_IN_NO_5]);
}



static void _stio_gpio_init_output1(void)	// OUTPUT1 GPIO Init
{
	ST_GpioSetup(output_port[0], 0); // gpio set
	ST_GpioDir(output_port[0], 1); // output mode set
	ST_GpioSetMsdk(TRUE);	// msdk enable(essential)	
	ST_GpioPullEn(output_port[0], TRUE);	// output_1 pull up, pull down set	
	//ST_GpioOut(output_port[0], 0); // set init value is high

	if(sw_reset == FALSE)
	{
		nz_output_status[0] = 0;
		ST_GpioOut(output_port[0], nz_output_status[0]); // set init value is high
		ST_GpioSetlatch();	// latch after output setting
	}
	else
	{
		ST_GpioOut(output_port[0], nz_output_status[0]); // set init value is high
	}
}

static void _stio_gpio_init_output2(void)	// OUTPUT2 GPIO Init
{
	ST_GpioSetup(output_port[1], 0); // gpio set
	ST_GpioDir(output_port[1], 1); // output mode set
	ST_GpioPullEn(output_port[1], TRUE);	// output_1 pull up, pull down set	
	//ST_GpioOut(output_port[1], 0); // set init value is high

	if(sw_reset == FALSE)
	{
		nz_output_status[1] = 0;
		ST_GpioOut(output_port[1], nz_output_status[1]); // set init value is high
		ST_GpioSetlatch();	// latch after output setting
	}
	else
	{
		ST_GpioOut(output_port[1], nz_output_status[1]); // set init value is high
	}
}

static void _stio_gpio_init_output3(void)	// OUTPUT3 GPIO Init
{
	ST_GpioSetup(output_port[2], 0); // gpio set
	ST_GpioDir(output_port[2], 1); // output mode set
	ST_GpioPullEn(output_port[2], TRUE);	// output_1 pull up, pull down set	
	//ST_GpioOut(output_port[2], 0); // set init value is high

	if(sw_reset == FALSE)
	{
		nz_output_status[2] = 0;
		ST_GpioOut(output_port[2], nz_output_status[2]); // set init value is high
		ST_GpioSetlatch();	// latch after output setting
	}
	else
	{
		ST_GpioOut(output_port[2], nz_output_status[2]); // set init value is high
	}
}

static void _stio_gpio_set_output1(bool status, bool active, bool lock)
{
	ST_GpioOut(output_port[0], (uint8)status);
	ST_GpioSetlatch();
	_stio_gpio_set_out_state(OUTPUT_NO_1, status, active, lock);

	nz_output_status[0] = status;
}

static void _stio_gpio_set_output2(bool status, bool active, bool lock)
{
	ST_GpioOut(output_port[1], (uint8)status);
	ST_GpioSetlatch();
	_stio_gpio_set_out_state(OUTPUT_NO_2, status, active, lock);
	
	nz_output_status[1] = status;
}

static void _stio_gpio_set_output3(bool status, bool active, bool lock)
{
	ST_GpioOut(output_port[2], (uint8)status);
	ST_GpioSetlatch();
	_stio_gpio_set_out_state(OUTPUT_NO_3, status, active, lock);
	
	nz_output_status[2] = status;
}
/*****************************
	 Ignition state set -> ignition type???�라 변경되기에, ignition on/off message 처리???�에 set ?�다.
	 (interrupt�?state set?� input ?�역�??�정)
*****************************/
static void _stio_gpio_set_in_state(uint8 no, bool state)
{
	switch(no)
	{
		case EVENT_IGNITION :	//	0 : ignition // E¸·I Au°i CØ¾ßCO. ¹YAu½AAN C￥±a CØ¾ß ¸AA½
			//state = stio_gpio_bool_algebra_reverse(state);
			if(state == FALSE)
				in_state &= 0xfe;	// 1111 1110
			else
				in_state |= 0x01;	// 0000 0001	
			break;
		
		case EVENT_IN_NO_1 : 	//	1 : input1	( input state 1 : ground shorted, 0 : opened)
			state = stio_gpio_bool_algebra_reverse(state);
			if(state == FALSE)
				in_state &= 0xfd;	//	1111 1101
			else
				in_state |= 0x02;	//	0000 0010
			break;
		
		case EVENT_IN_NO_2 : 	//	2 : input2	( input state 1 : ground shorted, 0 : opened)
			state = stio_gpio_bool_algebra_reverse(state);
			if(state == FALSE)
				in_state &= 0xfb;	//	1111 1011
			else
				in_state |= 0x04;	//	0000 0100
			break;
		
		case EVENT_IN_NO_3 : 	//	3 : input3	( input state 1 : ground shorted, 0 : opened)
			state = stio_gpio_bool_algebra_reverse(state);
			if(state == FALSE)
				in_state &= 0xf7;	//	1111 0111
			else
				in_state |= 0x08;	//	0000 1000	
			break;
		
		case EVENT_IN_NO_4 : 	//	4 : input4	( input state 1 : ground shorted, 0 : opened)
			state = stio_gpio_bool_algebra_reverse(state);
			if(state == FALSE)
				in_state &= 0xef;	//	1110 1111
			else
				in_state |= 0x10;	//	0001 0000	
			break;
		
		case EVENT_IN_NO_5 :	//	5 : input5	( input state 1 : ground shorted, 0 : opened)
			state = stio_gpio_bool_algebra_reverse(state);
			if(state == FALSE)
				in_state &= 0xdf;	//	1101 1111
			else
				in_state |= 0x20;	//	0010 0000	
			break;

		default :

		break;
	}
}


static void _stio_gpio_set_out_state(uint8 no, bool state, bool active, bool lock)
{
	bool set_state = FALSE;
	
	if(lock == TRUE)
	{
		set_state = active;
	}
	else
	{
		set_state = state;
	}
		
	switch(no)
	{
		case OUTPUT_NO_1 :	//	0 : output1
			if(set_state == FALSE)
				out_state &= 0xfe;	//	1111 1110
			else
				out_state |= 0x01;	//	0000 0001
		break;
			
		case OUTPUT_NO_2 :	//	1 : output2
			if(set_state == FALSE)
				out_state &= 0xfd;	//	1111 1101
			else
				out_state |= 0x02;	//	0000 0010
		break;
			
		case OUTPUT_NO_3 :	//	2 : output3
			if(set_state == FALSE)
				out_state &= 0xfb;	//	1111 1011
			else
				out_state |= 0x04;	//	0000 0100
		break;
			
		case OUTPUT_NO_4 :	//	3 : output4
			if(set_state == FALSE)
				out_state &= 0xf7;	//	1111 0111
			else
				out_state |= 0x08;	//	0000 1000
		break;

		default :

		break;
	}
}

static void _stio_gpio_check_sw_reset(void)
{
	if((io_check1 != 0xA5A5A5A5) || (io_check2 != 0x5A5A5A5A))	
	{	// hw reset
		io_check1 = 0xA5A5A5A5;
		io_check2 = 0x5A5A5A5A;
	}
	else
	{	// sw reset
		sw_reset = TRUE;
		memset(sw_reset_check, TRUE, sizeof(sw_reset_check));
	}
}

void _stio_gpio_init_rs232(void)
{
	uint16 rs232_en_gpio;
	
	ST_PRINT("gpio init for rs232");

	rs232_en_gpio = RS232_EN_GPIO_PORT_1;
	
	ST_GpioSetup(rs232_en_gpio, 0); // gpio set
	ST_GpioDir(rs232_en_gpio, 1); // output mode set
	ST_GpioPullSel(rs232_en_gpio, FALSE);
	ST_GpioPullEn(rs232_en_gpio, TRUE);	// output_1 pull up, pull down set	
	ST_GpioOut(rs232_en_gpio, (uint8)FALSE);
	//ST_GpioOut(rs232_en_gpio, (uint8)TRUE);

	ST_GpioSetup(RS232_RX_GPIO_PORT, 3); // uart3 rx
	ST_GpioSetup(RS232_TX_GPIO_PORT, 3); // uart3 rx
}

void stio_gpio_init(void)
{	
	ST_PRINT( "########## LED & IO PORT INIT ##########");

	switch(stcommon_model_get_hw_id())
	{
	case MODEL_HW_ST4300: 
	case MODEL_HW_ST3300:
		input_port = st4300_input_gpio;
		output_port = st4300_output_gpio;
		break;

	case MODEL_HW_ST3300R:
	case MODEL_HW_ST3300RE:
		input_port = st3300R_input_gpio;
		output_port = st3300R_output_gpio;
		break;
		
	case MODEL_HW_ST4340:
	case MODEL_HW_ST3340:
	case MODEL_HW_ST4340R: 
	case MODEL_HW_ST3340R: 
	case MODEL_HW_ST4310P:		
		input_port = st4340_input_gpio;
		output_port = st4340_output_gpio;
		break;

	case MODEL_HW_ST4310: 
	case MODEL_HW_ST4310U: 
	case MODEL_HW_ST4310W: 
	case MODEL_HW_ST4310R: 
	case MODEL_HW_ST4310S: 
	case MODEL_HW_ST4310M: 
		input_port = st4310_input_gpio;
		output_port = st4310_output_gpio;
		break;		
		
	// jwpark_4500
	case MODEL_HW_ST4500: 
		input_port = st4500_input_gpio;
		output_port = st4500_output_gpio;
		break;
		
// <<<<< GIT
	case MODEL_HW_ST4340H: 
		input_port = st4340H_input_gpio;
		output_port = st4340H_output_gpio;
		break;
// GIT >>>>>        
	default:
		input_port = st4300_input_gpio;
		output_port = st4300_output_gpio;
		break;
	}
	
	_stio_gpio_init_gpsled();	//	 GPS Led gpio init
	_stio_gpio_init_gprsled();	//	 GPRS Led gpio init	

	if(stcommon_model_get_port_type(MODEL_IGN_PORT) != OE_NO_USE)
		_stio_gpio_init_ignition();

	ST_PmuVSIM1LdoEnable(TRUE);
	ST_GpioSetMsdk(TRUE);	// msdk enable(essential) : SQA-3607	

	_stio_gpio_check_sw_reset();

	// jwpark_4500
#ifdef __ST4500_APP__
	if(stcommon_model_hasRS232() == TRUE || stcommon_model_get_hw_id() == MODEL_HW_ST4500)
#else
	if(stcommon_model_hasRS232())
#endif
	{
		_stio_gpio_init_rs232();
	}

	stio_gpio_adc_enable(TRUE); /* ADC Enable */	
}


void stio_gpio_in_out_init(void)
{
	if(stcommon_model_get_port_type(MODEL_IN1_PORT) != OE_NO_USE) 
	{
		_stio_gpio_init_input1();	//	 INPUT 1 gpio init
		stio_gpio_input1_set_to_input();
		//stio_gpio_input1_set_to_ignition();
	}
	if(stcommon_model_get_port_type(MODEL_IN2_PORT) != OE_NO_USE) 
		_stio_gpio_init_input2();	//	 INPUT 2 gpio init
	if(stcommon_model_get_port_type(MODEL_IN3_PORT) != OE_NO_USE) 
		_stio_gpio_init_input3();	//	 INPUT 3 gpio init
	if(stcommon_model_get_port_type(MODEL_IN4_PORT) != OE_NO_USE) 
		_stio_gpio_init_input4();	//	 INPUT 4 gpio init
	if(stcommon_model_get_port_type(MODEL_IN5_PORT) != OE_NO_USE) 
		_stio_gpio_init_input5();	//	 INPUT 5 gpio init

	if(stcommon_model_get_port_type(MODEL_OUT1_PORT) != OE_NO_USE) 	
		_stio_gpio_init_output1();	//	output 1 gpio init
	if(stcommon_model_get_port_type(MODEL_OUT2_PORT) != OE_NO_USE)	
		_stio_gpio_init_output2();	//	output 2 gpio init
	if(stcommon_model_get_port_type(MODEL_OUT3_PORT) != OE_NO_USE)		
		_stio_gpio_init_output3();	//	output 3 gpio init		
		
	stio_ctrl_init();
}



/**********************************************************
 					extern function 
**********************************************************/

bool stio_gpio_bool_algebra_reverse(bool bool_data)
{
	if( bool_data == TRUE )
	{
		bool_data = FALSE;
	}
	else
	{
		bool_data = TRUE;
	}

	return bool_data;
}



void stio_gpio_set_gpsled(bool status)
{
	ST_GpioSetGpsled(status);
}

void stio_gpio_set_gprsled(bool status)
{
	ST_GpioSetkpled(status);
}


void stio_gpio_set_output(uint8 output_num, bool status, bool active, bool lock)
{
	if(( output_num == 0 ) && (sw_reset == FALSE))
		_stio_gpio_set_output1(status, active, lock);	
	else if(( output_num == 1 ) && (sw_reset == FALSE))
		_stio_gpio_set_output2(status, active, lock);	
	else if(( output_num == 2 ) && (sw_reset == FALSE))
		_stio_gpio_set_output3(status, active, lock);	
}

void stio_gpio_input1_set_to_input(void)
{
	ST_GpioSetup(IGN_DC_BLOCK_PORT, 0);
	ST_GpioDir(IGN_DC_BLOCK_PORT, 1);
	ST_GpioOut(IGN_DC_BLOCK_PORT, 1);

	ST_GpioPullSel(input_port[1].gpio_no, TRUE);
	ST_GpioPullEn(input_port[1].gpio_no, TRUE);
	ST_GpioSetup(input_port[1].gpio_no, input_port[1].alt_no);
	//GPIO_set_pupd_R(INPUT1_GPIO_PORT, TRUE, TRUE, TRUE);
	//EINT_Set_HW_Debounce(INPUT1_EINT_NO, 100);
	ST_EintSetup(input_port[1].eint_no, TRUE, eint_polarity[EVENT_IN_NO_1], HW_DEB_TIME, _stio_gpio_input1_hisr);	
	ST_EintEnable(input_port[1].eint_no);

}

void stio_gpio_input1_set_to_ignition(void)
{
	ST_GpioSetup(IGN_DC_BLOCK_PORT, 0);
	ST_GpioDir(IGN_DC_BLOCK_PORT, 1);
	ST_GpioOut(IGN_DC_BLOCK_PORT, 1);
	ST_GpioPullEn(IGN_DC_BLOCK_PORT, TRUE);
	ST_GpioPullSel(IGN_DC_BLOCK_PORT, TRUE);

	ST_GpioSetup(input_port[1].gpio_no, 0);
	ST_GpioPullEn(input_port[1].gpio_no, FALSE);
}

void stio_gpio_input1_set_to_ignition_sjsongtest(void)
{
	ST_GpioSetup(IGN_DC_BLOCK_PORT, 0);
	ST_GpioDir(IGN_DC_BLOCK_PORT, 1);
	ST_GpioOut(IGN_DC_BLOCK_PORT, 1);
	
	ST_GpioPullEn(IGN_DC_BLOCK_PORT, TRUE);	
	ST_GpioPullSel(IGN_DC_BLOCK_PORT, TRUE);
	
	ST_PRINT("SSJ INPUT PORT : %d", input_port[0].gpio_no);
}

void stio_gpio_adc_enable(bool enable)
{
#ifdef BG96_ADC_SUPPORT
	if(enable)
	{
		ST_GpioSetup(BG96_ADC_EN_PORT, 0);
		ST_GpioDir(BG96_ADC_EN_PORT, 1);
		ST_GpioOut(BG96_ADC_EN_PORT, 1);
		ST_GpioPullEn(BG96_ADC_EN_PORT, TRUE);
		ST_GpioPullSel(BG96_ADC_EN_PORT, TRUE);
	}
	else
	{
		ST_GpioSetup(BG96_ADC_EN_PORT, 0);
		ST_GpioDir(BG96_ADC_EN_PORT, 1);
		ST_GpioOut(BG96_ADC_EN_PORT, 0);
		ST_GpioPullEn(BG96_ADC_EN_PORT, TRUE);
		ST_GpioPullSel(BG96_ADC_EN_PORT, TRUE);
	}
#endif	
}

uint8 stio_gpio_get_in_state(void)
{
	return in_state;
}

uint8 stio_gpio_get_out_state(void)
{
	return out_state;
}

void stio_gpio_set_after_boot(uint8 input1_type)
{
	uint8 hw_id = 0;
	hw_id = stcommon_model_get_hw_id();
	
	if(sw_reset == TRUE)
		sw_reset = FALSE;

	if((hw_id >= MODEL_HW_ST4310U) && (hw_id <= MODEL_HW_ST4310M))
	{
		ST_PRINT("[STIO] %s, input 1 type : %d", __FUNCTION__, input1_type);
		if(input1_type != INPUT_USE_IGNITION)		
			return;
	}
	
	if( ST_GpioIn(input_port[EVENT_IGNITION].gpio_no) == 0 )	// ignition on state
	{		
		eint_polarity[EVENT_IGNITION] = FALSE;
		_stio_gpio_change_input_status(EVENT_IGNITION, eint_polarity[EVENT_IGNITION]);	
		ST_PRINT("SSJ AFTERBOTT ign on");
	}
	else		// ignition off state
	{
		eint_polarity[EVENT_IGNITION] = TRUE;
		ST_PRINT("SSJ AFTERBOTT ign off");		
	}

	//_stio_gpio_set_in_state(EVENT_IGNITION, eint_polarity[EVENT_IGNITION]);	

	ST_PRINT("SSJ ign : %d, eint port : %d", eint_polarity[EVENT_IGNITION], input_port[0].eint_no);
	
	eint_polarity[EVENT_IGNITION] = stio_gpio_bool_algebra_reverse(eint_polarity[EVENT_IGNITION]);
	ST_EintSetup(input_port[0].eint_no, TRUE, eint_polarity[EVENT_IGNITION], HW_DEB_TIME, _stio_gpio_ignition_hisr);
}


void stio_gpio_set_ignition(bool state)
{
	_stio_gpio_set_in_state(EVENT_IGNITION, state);
}

bool stio_gpio_get_swreset_state(bool clear, uint8 num)
{
	if(clear == TRUE)
		sw_reset_check[num] = FALSE;	// Clear
	
	return sw_reset_check[num];
}

bool stio_gpio_input_state(uint16 num)
{
	bool in_state = FALSE;

	in_state = stio_gpio_bool_algebra_reverse(eint_polarity[num]);
	
	return in_state;
}

bool stio_gpio_output_state(uint16 num)
{
	bool output_state = FALSE;

	output_state = (bool)((out_state >> num) & 0x01);

	return output_state;
}


static void stio_gpio_rs232rx_eint_hdlr(void)
{
	ST_SleepRS232Enable(FALSE);

	ST_EintDisable(RS232_RX_EINT);
	ST_GpioSetup(RS232_RX_GPIO_PORT, 3); 
	ST_PRINT("[STIO] RS232 Sleep Disable");
	ST_SendMsg(MOD_STIO, MSG_ID_ST_IO_RS232_RX_EINT, NULL, NULL);
}

void	stio_gpio_rs232rx_eint(bool enable)
{
	if(enable)
	{
	 	ST_GpioSetup(RS232_RX_GPIO_PORT, 1);
		ST_EintSetup(RS232_RX_EINT, 1, 1, 0, stio_gpio_rs232rx_eint_hdlr);
	}
	else
	{
		ST_EintDisable(RS232_RX_EINT);
	 	ST_GpioSetup(RS232_RX_GPIO_PORT, 3);
	}
}

static uint16 stio_gpio_get_rs232_en_gpio(void)
{
	return RS232_EN_GPIO_PORT_1;
}

void stio_gpio_rs232_en(bool enable)
{
	uint16 rs232_en_gpio;
	
	rs232_en_gpio = stio_gpio_get_rs232_en_gpio();
	ST_GpioOut(rs232_en_gpio, (uint8)enable);
}

void stio_gpio_set_input_use_ignition(bool ignition)	// TRUE : IGN, FALSE : INPUT
{
	uint8 hw_id = 0, changed_chk = 2;
	hw_id = stcommon_model_get_hw_id();

	if((hw_id < MODEL_HW_ST4310U) || (hw_id > MODEL_HW_ST4310M) 
		|| (changed_chk != 2) && (changed_chk == (uint8)ignition))
		return;

	ST_PRINT("SSJ input use ignition");

	if(ignition == TRUE)
	{
		stio_gpio_input1_set_to_ignition_sjsongtest();
		
		if(changed_chk == 2)
		{
			_stio_gpio_init_ignition();
		}
		
	}
	else
	{
		stio_gpio_input1_set_to_input();
	}
	
	changed_chk = (uint8)ignition;
}