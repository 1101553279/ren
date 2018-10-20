#include "com.h"
#include "ptime.h"
#include "blist.h"
#include "string.h"
#include "chip_pca.h"
#include "debug.h"
#include "net.h"
#include "led.h"

#define NODE_NBR      20  	//可以同时使用20个node



// internal function declaration
static void node_init(void);
static struct com_node *com_node_alloc( void );
static void com_node_free( struct com_node *node );

// node space declaration
static struct com_node node_base[ NODE_NBR ];

static void dcom_uic78_init( struct uic78 *uic)
{
    if( 0 == uic)
        return;

    // default: not check
    uic->flag = 0;
    uic->ocur = 0;
    uic->onu = 0;			// output how many time uic
    uic->otime = ptime();

    uic->irising = 0;
    uic->ifalling = 0;
    uic->itime = ptime();	// time recording last input

// set default uic78 out value
    pca_out(PCA_ID_OUT_UIC78, OUT_UIC78_D);

    return;
}

/* init dcom basic information */
void com_info_init( struct com_info *ci )
{
    if( 0 == ci )
        return;

    memset(ci, 0, sizeof(*ci) );

    ci->start = 0xff;
    ci->rd_cab = 0x30;
//	ci->rd_type = 0xf8;
//	ci->rd_no = 0x30;
    ci->sd_cab = 0x30;
	ci->sd_dev = 0x04;
	ci->sd_no = dev_addr_is();
//	ci->cmd = 0;
    ci->len = 0x34;
//	ci->op = 0x30;
    ci->dc_no = 0x30;
    ci->resv1 = 0x30;
    ci->resv2 = 0x30;
    ci->crc1 = 0x30;
    ci->crc2 = 0x30;
    ci->stop = 0xfe;

    return;
}

// dacu self call
static void dcom_wait_init( struct dcom_wait *dw)
{
    if( 0 == dw )
        return;

    dw->flag = 0;
    dw->time = ptime();
    INIT_LIST_HEAD( &(dw->dq) );
//	memset( &dw->info, 0, sizeof(dw->info) );
//    com_info_init( &dw->info);

    return;
}


void com_init(struct com *com)
{
    if( 0 == com)
        return;

    dcom_uic78_init( &(com->uic) );

    com_info_init( &(com->cinfo) );

    led_init( &(com->pled), LN_PCOM );

    led_init( &(com->dled), LN_DCOM );

// pecu wait queue
    INIT_LIST_HEAD( &(com->pq) );

// dacu wait queue
//    INIT_LIST_HEAD( &(com->dq) );

// list node space init
    node_init();		// init node array

    dcom_wait_init( &(com->sd) );

//	dump_com(com);

    return;
}

/* uic78 is idling */
u8_t uic78_idling( struct uic78 *uic)
{
    return 0 == uic->flag;
}

/* uic78 is inputing */
u8_t uic78_inputing( struct uic78 *uic)
{
    return 1 == uic->flag;
}

u8_t uic78_input_timeout(struct uic78 *uic)
{
	return (ptime()-uic->itime > 300);
}

/* uic78 is outputing */
u8_t uic78_outputing( struct uic78 *uic)
{
    return 2 == uic->flag;
}

u8_t uic78_output_timeout(struct uic78 *uic)
{
	u8_t flag = 0;
	
	flag = (ptime() - uic->otime > 20) ? 1: 0;
	
	uic->otime = ptime();
	
	return flag;
}


/*  start uic78 input check 
	record input edge according to type
*/
void uic78_intput_set(struct uic78 *uic, u8_t type)
{	// after output 50ms, can check uic input
	if( 0 == uic->flag && ptime() - uic->otime > 50)
	{
		uic->irising = 0;
		uic->ifalling = 0;
		uic->flag = 1;
		uic->itime = ptime();
		
		if( 1 == type )
			uic->irising++;
		else
			uic->ifalling++;
		dout( "%s: uic check start! -> start time= %d\r\n",
		__func__, uic->itime );
	}
	else if( 1 == uic->flag )
	{
		if( 1 == type )
			uic->irising++;
		else
			uic->ifalling++;
		dout( "%s: uic in checking! -> rising = %d, falling = %d\r\n",
		__func__, uic->irising, uic->ifalling );
	}
	else if( 2 == uic->flag )
	{
	   dout( "%s: uic in outputing! -> ignore\r\n", __func__);
	}
	
	return;
}

/* check uic78 input event cmd: con, setup, hangup */
u8_t uic78_input_check(struct uic78 *uic)
{
	u8_t cmd = UIC78_IN_NONE;
	
	if(1 == uic->flag && (ptime()-uic->itime) > 300)
	{
		dout("%s: uic78 input cal! -> rising = %d, falling = %d\r\n",
		 __func__, uic->irising, uic->ifalling);
		
		if( uic->irising >=3 && uic->ifalling >= 3)
			cmd = UIC78_IN_HANGUP;
		else if( 1 == uic->irising && 0 == uic->ifalling )
			cmd = UIC78_IN_CON;
		else if( 0 == uic->irising && 1 == uic->ifalling )
			cmd = UIC78_IN_SETUP;
		
		uic->ifalling = 0;
		uic->irising = 0;
		uic->flag = 0;
	}
	
	return cmd;
}

/* control uic78 output signals */
void uic78_output_set(struct uic78 *uic, u8_t cmd)
{
    switch( cmd )
    {
    case UIC78_OUT_CON:
        uic->ocur = 1;
        uic->onu = 1;
        break;

    case UIC78_OUT_SETUP:
		uic->ocur = 0;
		uic->onu = 1;
        break;

    case UIC78_OUT_HANGUP:
		uic->ocur = 1;
		uic->onu = 6;		// 3 pluse
        break;

    default:
        return;
    }
	
	uic->otime = ptime();
    uic->flag = 2;

    return;
}

/* check whether need to output */
void uic78_output_check(struct uic78 *uic)
{
	u16_t lv;
	
	if( 2 == uic->flag && ptime()-uic->otime >30)
	{
		uic->otime = ptime();
		if( 0 == uic->ocur)
		{
			lv = OUT_UIC78_L;
			uic->ocur = 1;
		}
		else
		{
			lv = OUT_UIC78_H;
			uic->ocur = 0;
		}
		
		pca_out( PCA_ID_OUT_UIC78, lv);
		
		uic->onu--;
		// decide uic output whether is finished
		if( 0 == uic->onu )
		{
			uic->ocur = 0;
			uic->otime = ptime();	// record time when finished
			uic->flag = 0;			// set to idle
		}
	}
	
	return;
}


/* decide com whether is idling accordding to type*/
u8_t com_idling(struct com *com, u8_t type)
{
    if( COM_IS_P == type )
        return list_empty(&com->pq);
    else
        return (0 == com->sd.flag) && list_empty(&com->sd.dq);
}

/* decide whether has waitting */
u8_t com_waitting(struct com *com, u8_t type)
{
    if( COM_IS_P == type)
        return !list_empty(&com->pq);
    else if( COM_IS_D == type)
        return (2 == com->sd.flag && !list_empty(&com->sd.dq));

    return 0;
}

/* decide com whether is calling accordding to type*/
u8_t com_calling(struct com *com, u8_t type)
{
    if( COM_IS_D == type)
        return (1 == com->sd.flag) && !list_empty(&com->sd.dq);
    else
        return 0;
}

void to_dcom_idle(struct dcom_wait *dw)
{
	dw->flag = 0;
	dw->time = ptime();
}

void to_dcom_call(struct dcom_wait *dw)
{
	dw->flag = 1;
	dw->time = ptime();
}

u8_t com_call_timeout(struct dcom_wait *dw)
{
	return ptime() - dw->time > 10000;
}

void to_dcom_wait(struct dcom_wait *dw)
{
	dw->flag = 2;
	dw->time = ptime();
}

struct led *com_led_is(struct com *com, u8_t type)
{
    if( COM_IS_P == type)
        return &com->pled;
    else
        return &com->dled;
}

struct list_head *com_list_is(struct com *com, u8_t type)
{
    if( COM_IS_P == type)
        return &com->pq;
    else
        return &com->sd.dq;
}


//void com_uic_op(struct





u8_t rd_info_cb(struct com_info *dst, struct com_info *src)
{
    return dst->rd_type == src->rd_type &&
           dst->rd_no == src->rd_no;
}

u8_t sd_info_cb(struct com_info *dst, struct com_info *src)
{
    return dst->sd_dev == src->sd_dev &&
           dst->sd_no == src->sd_no;
}


//find com_node in list
struct com_node* com_node_find(struct list_head *head, struct com_info *cmp, node_find_cb_t cb )
{
    struct list_head *pos;
    struct list_head *n;
    struct com_node *cn = 0;
	u8_t flag = 0;

    // literate the list
    list_for_each_safe(pos, n, head)
    {
        cn = list_entry( pos, struct com_node, head );
        // find one entry
        if( cb(&cn->node, cmp) )
        {
			flag = 1;
			break;
        }
    }

    return (1 == flag ) ? cn: 0;
}

//add com_node to list
void com_node_add(struct list_head *head, struct com_info *tt )
{
    struct com_node *cn;

    // alloc node space
    cn = com_node_alloc();

    // hand com node
    memcpy(&cn->node, tt, sizeof(*tt) );

    // add to list tail
    list_add_tail(&(cn->head), head );

    return;
}

//del com_node from list
void com_node_del(struct list_head *head, struct com_node *cn )
{
    // delete for list
    list_del( &(cn->head) );

    // free com node space
    com_node_free( cn );

    return;
}


/* ========== node management =================*/
static void node_init(void)
{
    u8_t i = 0;

    for( i = 0; i < NODE_NBR; i++ )
        node_base[i].use = 0;		// not in use

    return;
}

static struct com_node *com_node_alloc( void )
{
    struct com_node *node = 0;
    u8_t i = 0;

    for( i = 0; i < NODE_NBR; i++ )
    {
        if( 0  == node_base[i].use )
        {
            node_base[i].use = 1;		// set in use
            node = &node_base[i];
            break;
        }
    }

    return node;
}

static void com_node_free( struct com_node *node )
{
    if( 0 == node )
        return;

    memset( node, 0, sizeof(struct com_node) );

    node->use = 0;			// set not in use

    return;
}

// only for debug
void dump_com_uic(struct uic78 *uic)
{
    if(0 == uic)
        return;

    dout("\t\t uic part\r\n" );
    dout("%-10s %-10d %-20s\r\n", "flag", uic->flag, "idle/input/output(0 ~ 2)" );
    dout("%-10s %-10d %-20s\r\n", "ocur", uic->ocur, "L/H(0~1)" );
    dout("%-10s %-10d %-20s\r\n", "onu", uic->onu, "number(times)" );
    dout("%-10s %-10d %-20s\r\n", "otime", uic->otime, "last output time" );
    dout("%-10s %-10d %-20s\r\n", "irising", uic->irising, "rising edge" );
    dout("%-10s %-10d %-20s\r\n", "ifalling", uic->ifalling, "falling edge" );
    dout("%-10s %-10d %-20s\r\n", "itime", uic->itime, "input cal time" );

    return;
}

void dump_com_info_title(char *name)
{
    dout("%-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s"
         "%-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s\r\n",
         name, "start", "rd_cab", "rd_type", "rd_no", "sd_cab", "sd_dev", "sd_no",
         "cmd", "len", "op", "dc_no", "resv1", "resv2", "crc1", "crc2", "stop" );

    return;
}

void dump_com_info_data(struct com_info *ci, u8_t no)
{
    if( 0 == ci )
        return;

    dout("%-6d %#-6x %#-6x %#-6x %#-6x %#-6x %#-6x %#-6x %#-6x"
         "%#-6x %#-6x %#-6x %#-6x %#-6x %#-6x %#-6x %#-6x\r\n",
         no, ci->start, ci->rd_cab, ci->rd_type, ci->rd_no, ci->sd_cab, ci->sd_dev, ci->sd_no,
         ci->cmd, ci->len, ci->op, ci->dc_no, ci->resv1, ci->resv2, ci->crc1, ci->crc2, ci->stop );

    return;
}

void dump_com_info(struct com_info *ci, char *name)
{
    if( 0 == ci )
        return;

    dout("com info %s part\r\n", name );
    dump_com_info_title("no");
    dump_com_info_data(ci, 1);

    dout("\r\n" );

    return;
}

void dump_list(struct list_head *head, char *name)
{
    u8_t no = 0;
    struct list_head *pos;
    struct list_head *n;
    struct com_node *cn = 0;
    struct com_info *ci;


    if( 0 == head )
        return;

    dout("%s list part\r\n", name );
    if( list_empty(head) )
    {
        dout("\t%s list is empty!\r\n", name);
        return;
    }

    dump_com_info_title("no");

    // literate the list
    list_for_each_safe(pos, n, head)
    {
        no++;
        cn = list_entry( pos, struct com_node, head );
        ci = &cn->node;
//        dout("node: use: %d, prev = %#x, next = %#x\r\n",
//             cn->use, (u32_t)cn->head.prev, (u32_t)cn->head.next );
        dump_com_info_data( ci, no );
    }

    dout("\r\n" );

    return;
}

void dump_com_wait(struct dcom_wait *dw, char *name )
{
    if( 0 == dw)
        return;

    dout("%s wait part\r\n", name );
    dout("%-10s %d(%s)\r\n", "flag", dw->flag, "idle/self call/other wait(0 ~ 2)" );
    dout("%-10s %d\r\n", "time", dw->time );

    dump_list(&dw->dq, "call/wait" );

    dout("\r\n" );

    return;
}

void dump_com(struct com *cm )
{
    if(0 == cm)
        return;

    dout("\r\n****** com summary ******r\n" );

//	dump_com_uic( &cm->uic );
    dump_com_info( &cm->cinfo, "cinfo");
    dump_com_info( &cm->tt, "tt info" );
    dout("pcom / dcom led part\r\n");
    dump_led( &cm->pled);
    dump_led( &cm->dled);
    dump_list( &cm->pq, "pecu");
    dout("\r\n" );
    dump_com_wait( &cm->sd, "dcom wait" );
    dout("\r\n" );


    return;
}

void dump_node_base(void)
{
    u8_t in;

    dout("\r\n****** node base summary ******r\n" );
    dout("%-10s %-10s\r\n", "no", "use");

    for(in = 0; in < NODE_NBR; in++)
    {
        dout("%-10d %-10s\r\n", in+1, (0==node_base[in].use) ? "-":
             ((1==node_base[in].use)? "*":"error") );
    }

    return;
}

