#include "dcore_hand.h"
#include "debug.h"
#include <string.h>
#include "dloud.h"
#include "ptime.h"
#include "chip_pca.h"
#include "mic.h"
#include "dnet_multi.h"
#include "tip_src.h"

#define stat_is( st, cm ) ((st) == (cm))

#define WHO_DACU		0x04
#define WHO_PECU		0x05
#define WHO_PCU_GROUP	0xf6
#define WHO_DACU_GROUP	0xf7
#define WHO_REC_GROUP	0xf8

#define CMD_DACU_PECU	0x37
#define CMD_PCU			0x38
#define CMD_DACU_GROUP	0x36
#define CMD_REC_GROUP	0x34

#define DACU_PECU_OP_SETUP		0x31
#define DACU_PECU_OP_UNSETUP	0x32
#define PCU_OP_IN_START		((0x33<<8)|(0x31))
#define PCU_OP_IN_STOP		((0x33<<8)|(0x32))
#define PCU_OP_OUT_START	((0x34<<8)|(0x31))
#define PCU_OP_OUT_STOP		((0x34<<8)|(0x32))
#define DACU_GROUP_OP_CALL	(0X31)
#define DACU_GROUP_OP_UNCALL	(0x32)
#define REC_GROUP_OP_START	(0x31)
#define REC_GROUP_OP_STOP	(0x32)

struct multi_info {
    u8_t start;		// frame start
    u8_t rd_cab;    // receive device cab
    u8_t rd_type;   // receive device type
    u8_t rd_no;     // receive device number
    u8_t sd_cab;    // send device cab
    u8_t sd_dev;    // send device type
    u8_t sd_no;     // send device number
    u8_t cmd;		// command
    u8_t len;		// data len
    u8_t op;		// operator code
    u8_t dc_no;     // device channel number
    u8_t resv1;		// resverve bits
    u8_t resv2;
    u8_t crc1;		// crc check
    u8_t crc2;
    u8_t stop;		// frame end
};

static s8_t sf_com(struct list_head *head, struct com *com );
static void to_call_com_info_init(struct com_info *ci);
static void but_idle_to_call(struct dstat *st, struct com *com, u8_t type);
static void but_call_to_idle(struct dstat *st, struct com *com, u8_t type);
static void timeout_call_to_idle(struct dstat *st, struct com *com, u8_t type);
static void con_idle_to_wait(struct dstat *st, struct com *com, u8_t type);
static void uncon_wait_to_idle(struct dstat *st, struct com *com, u8_t type);
static void setup_call_to_setup(struct dstat *st, struct com *com, u8_t type);
static void but_wait_to_setup(struct dstat *st, struct com *com, u8_t type);
static void but_setup_to_idle(struct dstat *st, struct com *com, u8_t type);
static void unsetup_setup_to_idle(struct dstat *st, struct com *com, u8_t type);
static void frozen_wait_to_setup(struct dstat *st, struct com *com, u8_t type);
static void unfrozen_setup_to_idle(struct dstat *st, struct com *com, u8_t type);


static void multi_info_init(struct multi_info *info)
{
    if( 0 == info)
        return;

    memset(info, 0, sizeof(*info) );

    info->start = 0xff;
    info->rd_cab = 0x30;
//	info->rd_type = 0;
//	info->rd_no = 0;
    info->sd_cab = 0x30;
	info->sd_dev = 0x04;
	info->sd_no = dev_addr_is();
//	info->cmd = 0;
    info->len = 0x34;
//	info->op = 0;
    info->dc_no = 0x30;
    info->resv1 = 0x30;
    info->resv2 = 0x30;
    info->crc1 = 0x30;
    info->crc2 = 0x30;
    info->stop = 0xfe;

    return;
}

/*
all com message send out by this
who: send to who
op: do what
*/
static void send_to_multi(struct com_info *info, u8_t who, u16_t op)
{
    struct multi_info multi;

    if( 0 == info )
        return;

    multi_info_init( &multi );		// init space

    switch( who )
    {   // to dacu for pcom setup/hangup
		// to pecu for dcom setup/hangup
    case WHO_DACU:
    case WHO_PECU:
        multi.rd_type = info->sd_dev;
        multi.rd_no = info->sd_no;
        multi.cmd = CMD_DACU_PECU;
        break;

    // to pcu for pa in/out start/stop
    case WHO_PCU_GROUP:
        multi.rd_type = who;
        multi.rd_no = 0x30;
        multi.cmd = CMD_PCU;
        break;

    // to dacu group for dcom call/cancel call
    case WHO_DACU_GROUP:
        multi.rd_type = who;
        multi.rd_no = 0x30;
        multi.cmd = CMD_DACU_GROUP;
        break;

    // to rec for pa start/stop
    case WHO_REC_GROUP:
        multi.rd_type = who;
        multi.rd_no = 0x30;
        multi.cmd = CMD_REC_GROUP;
        break;

    default:
        dout( "%s: unreconize who: %d, op = %d ! -> ignore\r\n",
              __func__, who, op );
        return;		// do nothing
//			break;
    }
	
    if( WHO_PCU_GROUP != who)
        multi.op = op;
    else
    {
        multi.op = (u8_t)(op>>8);			// pa in/out
        multi.dc_no = (u8_t)(op&0xff);		// pa start/stop
    }
	
// send to network
    dnet_multi_send((u8_t*)&multi, 16);

    return;
}

/* com: button event handle */
void com_but( struct dstat *st, struct com *com, u8_t type)
{
    if( 0 == st || 0 == com )
        return;

    if( !dcom_is_idle(st) )
    {
        if( dcom_is_setup(st, type) )
        {
            dout( "%s: com setup && type is ok! -> hangup\r\n", __func__ );
            but_setup_to_idle(st, com, type);
        }
        else
		{
			dout("%s: com setup or type is not! -> ignore\r\n", __func__);
		}
    }
    else	/* dcom is idle */
    {
		if( com_waitting(com, type) )
		{
			dout( "%s: com dile && pecu/dacu wait list not empty! -> to com\r\n", __func__ );
			but_wait_to_setup(st, com, type );	// setup pcom
		}
		else if( COM_IS_D == type )		// dacu: self call & self cancel call
		{
			if(com_calling(com, type) )
			{
				dout( "%s: dcom but && in calling! -> cancel\r\n", __func__ );
				but_call_to_idle(st, com, type);
			}
			else if(com_idling(com, type ))
			{
				dout( "%s: dcom but && not in calling!-> to call\r\n", __func__ );
				but_idle_to_call(st, com, type);
			}
		}
    }


    return;
}

/*
type: 	1 -> 	uic78 rising
		0 -> 	uic78 falling
*/
void com_uic_in(struct com *com, u8_t type)
{
	uic78_intput_set(&com->uic, type);
	
    return;
}

u8_t com_con_unvalid(struct com_info *com)
{
	return ((0x04 == com->sd_dev ) && (com->sd_no < 0x36) );
}

u8_t com_setup_valid(struct com_info *com)
{
	return ((0x04 == com->sd_dev ) && (com->sd_no > 0x36) );
}


/*
type: COM_IS_P or COM_IS_D
*/
void com_net(struct dstat *st, struct com *com,
             struct com_info *in, u8_t type, u8_t id)
{
//	if( 0 == st || 0 == com || 0 == in )
//		return;

// copy com inforamtion: to com->tt 
	memcpy(&(com->tt), in, sizeof(*in) );
	
    switch( id )
    {
    case VCOM_ID_REQ_CON:
		if(com_con_unvalid(&com->tt) )
			dout("%s: current cab's dacu req connect by network! -> ignore\r\n", __func__);
		else
			con_idle_to_wait( st, com, type );
        break;

    case VCOM_ID_REQ_UNCON:		
		if(com_con_unvalid(&com->tt) )
			dout("%s: current cab's dacu req disconnect by network! -> ignore\r\n", __func__);
		else
			uncon_wait_to_idle( st, com, type );
        break;

    case VCOM_ID_REP_SETUP:
        if( /*self addr == recv address*/1/*list is not empty*/)
            setup_call_to_setup(st, com, type );
        else if( /*self addr != recv address*/1/*|| list is empty */ )
            frozen_wait_to_setup(st, com, type );
        break;

    case VCOM_ID_REP_HANGUP:
        if( /*self addr == recv address*/1/*&& list is not empty*/)
            unsetup_setup_to_idle(st, com, type );
        else if( /*self addr != recv address*/1/*|| list is empty */ )
            unfrozen_setup_to_idle(st, com, type );
        break;

    default:
        dout("%s: switch default! -> ignore\r\n", __func__ );
        break;
    }

    return;
}

/* dacu com all wait check */
static void com_check_wait(struct com *com)
{
    if( 0 == com )
        return;

    if( com_calling( com, COM_IS_D) )
    {   // check com call wait timeout: 10s
        if( com_call_timeout( &com->sd) )
        {
            dout( "%s: dacu call timeout!\r\n", __func__ );

			timeout_call_to_idle(0, com, COM_IS_D);
        }
    }
    return;
}


/* dacu com uic input/output check */
static void com_check_uic(struct com *com, struct dstat *stat)
{
    struct uic78 *uic;
	u8_t cmd = UIC78_IN_NONE;

    if( 0 == com || 0 == stat)
        return;

    uic = &com->uic;
    if( !uic78_idling(uic) )
    {   // intput check, > 300ms -> input check finished
        if( uic78_inputing(uic) )
        {
            dout("%s: uic78 input cal! -> rising = %d, falling = %d\r\n",
                 __func__, uic->irising, uic->ifalling);
			cmd = uic78_input_check(uic);
			switch( cmd)
			{
				case UIC78_IN_CON:
					dout("%s: uic in cmd: con! -> idle to wait\r\n", __func__ );
					con_idle_to_wait(stat, com, COM_IS_D);
					break;
				
				case UIC78_IN_SETUP:
					dout("%s: uic in cmd: setup! -> wait to setup\r\n", __func__ );
					setup_call_to_setup(stat, com, COM_IS_D);
					break;
				
				case UIC78_IN_HANGUP:
					dout("%s: uic in cmd: hangup! -> setup to idle\r\n", __func__ );
					unsetup_setup_to_idle(stat, com, COM_IS_D);
					break;
				
				default:
					dout("%s: uic in  cmd error! -> ignore\r\n", __func__);
					break;
			}
				
        }
        // output check, output every 50ms
        else if( uic78_outputing(uic) )
        {
//            dout("%s: com uic output! -> onu = %d, ocur = %d\r\n",
//                 __func__, uic->onu, uic->ocur);

			uic78_output_check(uic);
        }
    }

    return;
}

/* com period check */
void com_check(struct com *com, struct dstat *stat)
{
    if( 0 == com || 0 == stat)
        return;

// check dcom led
    led_check( &com->dled );

// check uic78 input && output
    com_check_uic( com, stat );

// check dcom call wait
    com_check_wait( com );

// loud tip check
    dloud_check();

// check pcom led
    led_check( &com->pled );

    return;
}

/* occ event handle */
void occ_op(struct dstat *stat, u8_t op)
{
    if( 1 == op && OLSN_ST_IDLE == stat->olsn)
    {
        dloud_con(UT_OLSN, UE_LSN);	// loud: occ listen start
        stat->olsn = OLSN_ST_SETUP;	// set occ listen start
        dout("%s: occ listen start!\r\n", __func__);
    }
    else if( 0 == op && OLSN_ST_SETUP == stat->olsn)
    {
        dloud_con(UT_OLSN, UE_CLOSE);	// loud: occ listen close
        stat->olsn = OLSN_ST_IDLE;		// set occ listen stop
        dout("%s: occ listen stop!\r\n", __func__);
    }

    return;
}

static void wait_pa(struct dstat *stat, struct pa *pa, u8_t type)
{
    struct led *led;
    struct com_info none;
    u8_t pa_is;
    u16_t todo;

    pa_is = (1==type) ? PA_IS_IN: PA_IS_OUT;
    todo = stat_is(PA_IS_IN, pa_is) ? PCU_OP_IN_START:PCU_OP_OUT_START;
    led = stat_is(PA_IS_IN, pa_is) ? &(pa->ledin): &(pa->ledout);

    // set led status -> led on
    led_con(led, LE_BLINK);

    // set timeout
    pa->flag = 1;
    pa->time = ptime();

    // send network information
    send_to_multi( &none, WHO_PCU_GROUP, todo);
    send_to_multi( &none, WHO_REC_GROUP, REC_GROUP_OP_START);

    // operate uic56 to high
    pa_uic_open();

    // set pa status -> setup
    stat->pa_st = PA_ST_WAIT;
    stat->pa_is = pa_is;

    return;
}

/*
pa change to setup
*/
static void setup_pa( struct dstat *stat, struct pa *pa)
{
    struct led *led;
    u8_t pa_is;

    pa_is = stat->pa_is;
    led = stat_is(PA_IS_IN, pa_is) ? &(pa->ledin): &(pa->ledout);

    // set led status -> led on
    led_con(led, LE_ON);

    // set loud close
    dloud_con(UT_PA, UE_PA);

    // set pa flag -> 0(not check)
    pa->flag = 0;
    pa->time = ptime();	// record stop time

    // set pa status -> setup
    stat->pa_st = PA_ST_SETUP;

    return;
}

static void hangup_pa(struct dstat *stat,  struct pa *pa)
{
    struct led *led;
    struct com_info none;
    u8_t pa_is;
    u16_t todo;

    pa_is = stat->pa_is;
    todo = stat_is(PA_IS_IN, pa_is) ? PCU_OP_IN_STOP:PCU_OP_OUT_STOP;
    led = stat_is(PA_IS_IN, pa_is) ? &(pa->ledin): &(pa->ledout);

    if( PA_ST_WAIT == stat->pa_st)
        led_con(led, LE_BK_OFF);
    else if( PA_ST_SETUP == stat->pa_st)
    {
        led_con(led, LE_ON_OFF);
        dloud_con(UT_PA, UE_CLOSE);
    }

    // send network information
    send_to_multi( &none, WHO_PCU_GROUP, todo);
    send_to_multi( &none, WHO_REC_GROUP, REC_GROUP_OP_STOP);

    // operate uic56 to high
    pa_uic_close();

    // set timeout check flag -> 0 ( not check)
    pa->flag = 0;
    pa->time = ptime();

    // set pa status -> setup
    stat->pa_st = PA_ST_IDLE;
    stat->pa_is = PA_IS_NONE;

    return;
}

/* decide pa event && stat is match */
static u8_t pa_match(u8_t stat, u8_t but)
{   // cur state is pa out && pa out but is pushed
    // cur state is pa in && pa in but is pushed
    return ( (PA_IS_OUT == stat && 2 == but) ||
             (PA_IS_IN == stat && 1 == but) );
}

/* all pa event op */
void pa_op(struct dstat *stat, struct pa *pa, u8_t op)
{
    u8_t pa_st;
    u8_t pa_is;

    pa_st = stat->pa_st;
    pa_is = stat->pa_is;

    if( PA_ST_IDLE != pa_st )
    {
        if(PA_ST_WAIT == pa_st && 3 == op)
        {
            // setup pa operation
            if( PA_IS_NONE != pa_is )
            {
                setup_pa(stat, pa);
                dout( "%s: uic idle! -> pa %s setup\r\n",
                      __func__, (stat_is(pa_is,PA_IS_IN))?"in":"out" );
            }
            else
                dout( "%s: uic idle! pa is error! -> ignore \r\n", __func__ );
        }
        else if( PA_ST_WAIT == pa_st || PA_ST_SETUP == pa_st  )
        {
            if( pa_match(pa_is, op) )
            {
                hangup_pa(stat, pa);
                dout( "%s: pa but! -> %s hangup\r\n",
                      __func__, (stat_is(pa_is,PA_IS_IN))?"in":"out");
            }
            else
                dout("%s: pa but! not match! -> ignore \r\n", __func__);
        }
        else
            dout("%s: pa is not idle, op error! -> ignore \r\n", __func__);
    }
    else if( PA_ST_IDLE == pa_st)
    {   // op valid decide: 1 -> pa in / 2 -> pa out
        if( 1 == op || 2 == op )
        {
            wait_pa(stat, pa, op);
            dout("%s: pa in but! -> to wait\r\n", __func__);
        }
        else
            dout("%s: pa op error! -> ignore\r\n", __func__);
    }
    else
        dout("%s: pa st error! -> ignore\r\n", __func__);

    return;
}

void pa_check(struct pa *pa, struct dstat *stat)
{
    if( 0 == pa || 0 == stat)
        return;

// led in/out blink check
    led_check( &pa->ledin );
    led_check( &pa->ledout );

    // timout: open pa
    if( (1 == pa->flag) && (ptime()- pa->time > 3000) )
    {
        setup_pa(stat, pa);
        dout( "%s: pa wait timeout! -> pa setup\r\n", __func__ );
    }

    return;
}

/*
ppt pushed/poped hanle
*/
void ppt_op(struct dstat *stat, u8_t op)
{
    if( 0 == stat)
        return;

    if( 1 == op )
    {
        if(stat_is(COM_ST_SETUP, stat->com_st) ||
                stat_is(PA_ST_SETUP, stat->pa_st) )
        {
            mic_open();
            dout( "%s: ppt pushed! -> open\r\n", __func__);
        }
        else
        {
            dout( "%s: stat is not in -> ignore\r\n", __func__);
        }
    }
    else if( 2 == op)
    {
        mic_close();
        dout( "%s: ppt poped! -> close\r\n", __func__);
    }

    return;
}

/*
broadcast listen signal handle
*/
void blsn_op(struct dstat *stat, u8_t op)
{
    if( 0 == stat )
        return;

// self pa -> do not listen   		// self pa idle -> listen
    if( stat_is(PA_ST_IDLE, stat->pa_st) )
    {
        if( 1 == op && stat_is(BLSN_ST_IDLE, stat->blsn) )
        {
            dloud_con(UT_BLSN, UE_LSN);
            stat->blsn = BLSN_ST_SETUP;
            dout( "%s: broadcast listen signal start! -> blsn open\r\n", __func__);
        }
        else if( 2 == op && stat_is(BLSN_ST_SETUP, stat->blsn))
        {
            dloud_con(UT_BLSN, UE_CLOSE);
            stat->blsn = BLSN_ST_IDLE;
            dout( "%s: broadcast listen signal stop! -> blsn close\r\n", __func__);
        }
        else
            dout("%s: op code error:%d or blsn stat error: %d! -> ignore\r\n",
                 __func__, op, stat->blsn );
    }
    else
        dout( "%s: self pa, op: %d, pa stat = %d! -> ignore\r\n",
              __func__, op, stat->pa_st );

    return;
}

/*================================= split ====================*/
static void to_call_com_info_init(struct com_info *ci)
{
	if( 0 == ci )
		return;
	
	com_info_init( ci );
	
	ci->rd_type = WHO_DACU_GROUP;
	ci->rd_no = 0x30;
	ci->cmd = CMD_DACU_GROUP;
	ci->op = DACU_GROUP_OP_CALL;
	
	return;
}

static void but_idle_to_call(struct dstat *st, struct com *com, u8_t type)
{
    struct led *led;
	struct list_head *head;
    struct com_info info;

    // only support dacu call
    if( COM_IS_D != type )
        return;

    // uic input checking, can not to call
    if(  COM_IS_D != type && 0 != uic78_idling(&com->uic) )
        return;

    led = com_led_is(com, type);
	head = com_list_is(com, type);
	
	// fill basic com 
	to_call_com_info_init( &info );
	
	com_node_add(head, &info );
	
    // send network information
    led_con( led, LE_BLINK );	// led to blink
    dloud_con(UT_COM, UE_TIP);	// loud: tip

    // set uic78 out
	uic78_output_set(&com->uic, UIC78_OUT_CON);
	
    // send network information: dcom call
    send_to_multi( &info, WHO_DACU_GROUP, DACU_GROUP_OP_CALL);

//	set com.sd == call	
	to_dcom_call( &com->sd);
	
	dump_com(com);
		
	
	return;
}

static void but_call_to_idle(struct dstat *st, struct com *com, u8_t type)
{
    struct led *led;
	struct list_head *head;
    struct com_node *node;
	struct com_info info;

    // only support dacu call
    if( COM_IS_D != type )
        return;

    // uic input checking, can not to call
    if(  COM_IS_D != type && 0 != uic78_idling(&com->uic) )
        return;

    led = com_led_is(com, type);
	head = com_list_is(com, type);

// 此处最好通过find list 实现,暂时先用此种方法
	node = list_first_entry(head, struct com_node, head);
	
	memcpy(&info, &node->node, sizeof(struct com_info) );
	
// free com node 
	com_node_del(head, node );
	
    // send network information
    led_con( led, LE_BK_OFF );	// led to blink
    dloud_con(UT_COM, UE_TIP_CLOSE);	// loud: tip

    // set uic78 out
	uic78_output_set(&com->uic, UIC78_OUT_HANGUP);
	
    // send network information: dcom uncall
    send_to_multi( &info, WHO_DACU_GROUP, DACU_GROUP_OP_UNCALL);

//	set com.sd == idle	
	to_dcom_idle( &com->sd);
	
	dump_com(com);
		
	return;
}

static void timeout_call_to_idle(struct dstat *st, struct com *com, u8_t type)
{
	but_call_to_idle(st, com, type);
	
	return;
}

static void con_idle_to_wait(struct dstat *st, struct com *com, u8_t type)
{
	struct list_head *head;
    struct led *led;

// get list node
    head = com_list_is(com, type);
    led = com_led_is(com, type);

// device is already in list
	if( 0 != com_node_find(head, &com->tt, sd_info_cb) )
	{
		dout("%s: con com is already in list!\r\n", __func__);
		return;
	}

	com_node_add(head, &com->tt);
	
    dout( "%s: sd_dev = %#x, sd_no = %#x connect!\r\n",
          __func__, com->tt.sd_dev, com->tt.sd_no );
	
    led_con( led, LE_BLINK );	// led : blink start
    dloud_con(UT_COM, UE_TIP);	// loud: tip start

	if( COM_IS_D == type)
		to_dcom_wait( &com->sd);
	
	dump_com( com);
	
	return;
}

// connect for com: com request cancel
static void uncon_wait_to_idle(struct dstat *st, struct com *com, u8_t type)
{
    struct list_head *head;
    struct com_node *cn = 0;
    struct led *led;

// get list node
    head = com_list_is(com, type);
	if( list_empty(head) )
		return;
	
    led = com_led_is(com, type);

    // hand com node	
	cn = com_node_find( head, &(com->tt), sd_info_cb );
	if( 0 == cn )
	{
		dout("%s: can't not find  uncon node! -> ignore\r\n", __func__);
		return;
	}

	com_node_del( head, cn );
	
	dout( "%s: sd_dev = %#x, sd_no = %#x disconnect!\r\n",
	  __func__, com->tt.sd_dev, com->tt.sd_no );
	
	
	led_con( led, LE_BK_OFF );			// led : blink close
	dloud_con(UT_COM, UE_TIP_CLOSE);	// loud: tip close
	
	if( COM_IS_D == type)
		to_dcom_idle( &com->sd);
	
	dump_com( com);
	
    return;
}

/* com setup && frozen part */
static s8_t sf_com(struct list_head *head, struct com *com )
{
    struct com_node *cn;

	cn = com_node_find(head, &com->tt, rd_info_cb);
	if( 0 == cn )
	{
		dout("%s: can not find compare node! -> ignore\r\n", __func__ );
		return -1;
	}

// copy information
	memcpy(&com->cinfo, &cn->node, sizeof(struct com_info) );
	
	// com node free to list
	com_node_del(head, cn);

    return 0 ;
}

// connect for com: com request cancel
static void setup_call_to_setup(struct dstat *st, struct com *com, u8_t type)
{
	struct list_head *head;
    struct led *led;

    // get pecu/dacu led
    led = com_led_is(com, type);

    // get pecu/dacu list head
    head = com_list_is(com, type);
	if( list_empty(head) )
	{
		dout("%s: list empty! -> ignore\r\n", __func__);
		return;
	}

	if( sf_com(head, com ) < 0)
		return;
	
    led_con( led, LE_ON );		// led: to on
    dloud_con(UT_COM, UE_COM);	// loud: to com

//    st->com_st = COM_ST_SETUP;	// com stat change to setup
//    st->com_is = type;			// set com type
	dcom_to_setup(st, type);
	
	if( COM_IS_D == type)
		to_dcom_idle( &com->sd);
	
	dump_com( com);
	
    return;
}

// connect for com: com request cancel
static void but_wait_to_setup(struct dstat *st, struct com *com, u8_t type)
{
	struct list_head *head;
    struct led *led;
    u8_t who;

    // get pecu/dacu led
    led = com_led_is(com, type);

	who = (COM_IS_P == type) ? WHO_PECU: WHO_DACU;
	
    // get pecu/dacu list head
    head = com_list_is(com, type);
	if( list_empty(head) )
	{
		dout("%s: list empty! -> ignore\r\n", __func__);
		return;
	}

	if( sf_com(head, com ) < 0)
		return;
	
    led_con( led, LE_ON );		// led: to on
    dloud_con(UT_COM, UE_COM);	// loud: to com

	if( COM_IS_D == type)
	{
		// set uic78 out
		uic78_output_set(&com->uic, UIC78_OUT_SETUP);

		to_dcom_idle( &com->sd);
	}
	
	send_to_multi( &(com->cinfo), who, DACU_PECU_OP_SETUP);

//    st->com_st = COM_ST_SETUP;	// com stat change to setup
//    st->com_is = type;			// set com type
	dcom_to_setup(st, type);

	dump_com( com);
	
    return;
}

// connect for com: com request cancel
static void but_setup_to_idle(struct dstat *st, struct com *com, u8_t type)
{
	struct led *led;
    u8_t who;

    led = com_led_is(com, type);
    who = (COM_IS_P == type)? WHO_PECU: WHO_DACU;

    led_con( led, LE_ON_OFF );		// led: on -> off
    dloud_con(UT_COM, UE_CLOSE);	// loud: com -> close

// set uic78 out
	if( COM_IS_D == type)
		uic78_output_set(&com->uic, UIC78_OUT_HANGUP);

    // send network information: com hangup
    send_to_multi( &(com->cinfo), who, DACU_PECU_OP_UNSETUP);

    // set com status
	dcom_to_idle(st);

	dump_com( com);
	
    return;
}


// connect for com: com request cancel
static void unsetup_setup_to_idle(struct dstat *st, struct com *com, u8_t type)
{
	struct led *led;

    led = com_led_is(com, type);

    led_con( led, LE_ON_OFF );		// led: on -> off
    dloud_con(UT_COM, UE_CLOSE);	// loud: com -> close

    // set com status
	dcom_to_idle(st);

	dump_com( com);
	
    return;
}


// connect for com: com request cancel
static void frozen_wait_to_setup(struct dstat *st, struct com *com, u8_t type)
{
	struct list_head *head;
    struct led *led;

    // get pecu/dacu led
    led = com_led_is(com, type);
	
    // get pecu/dacu list head
    head = com_list_is(com, type);
	if( list_empty(head) )
	{
		dout("%s: list empty! -> ignore\r\n", __func__);
		return;
	}

	if( sf_com(head, com ) < 0)
		return;
	
    led_con( led, LE_ON );		// led: to on
    dloud_con(UT_COM, UE_FROZEN);	// loud: to com

	if( COM_IS_D == type)
	{
		to_dcom_idle( &com->sd);
	}

//    st->com_st = COM_ST_SETUP;	// com stat change to setup
//    st->com_is = type;			// set com type
	dcom_to_setup(st, type);

	dump_com( com);
	
    return;
}

/* unfrozen: com status: setup -> idle */
static void unfrozen_setup_to_idle(struct dstat *st, struct com *com, u8_t type)
{
	struct led *led;

    led = com_led_is(com, type);

    led_con( led, LE_ON_OFF );		// led: on -> off
    dloud_con(UT_COM, UE_FROZEN_CLOSE);	// loud: com -> close

    // set com status
	dcom_to_idle(st);

	dump_com( com);
	
    return;
}




