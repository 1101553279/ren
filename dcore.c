#include "dcore.h"
#include "event.h"
#include "debug.h"
#include "dbg_cmd.h"
#include "com.h"
#include "ptime.h"
#include "mic.h"
#include "dloud.h"

//#define TOSTR( str )    "cmd_"#str
//#define CORE_CMD_INIT( name, id )    {TOSTR(name), id, hand_##name}
#define CORE_CMD_INIT( name, type )    {"cmd_"#name, type, hand_##name}

typedef void (*dcore_cb_t)(struct dcore *core, union event_info *value);

/* hand all event by callback*/
struct dcore_cb {
    char *name;
    u8_t type;		// event type
    dcore_cb_t cb;	// hand event callback
};


// for cmd table
static void hand_none(struct dcore *core, union event_info *va);
static void hand_diag(struct dcore *core, union event_info *va);
static void hand_ip(struct dcore *core, union event_info *va);
static void hand_link(struct dcore *core, union event_info *va);
static void hand_com(struct dcore *core, union event_info *va);
static void hand_occ(struct dcore *core, union event_info *va);
static void hand_pa(struct dcore *core, union event_info *va);
static void hand_blsn(struct dcore *core, union event_info *va);
static void hand_ppt(struct dcore *core, union event_info *va);
//static void hand_max(struct dcore *core, union event_info *va);

/* for init core_init */
static struct dcore_cb cmd_table[ MAX_EVENT ] =
{
    CORE_CMD_INIT( none, EVENT_NONE),
    CORE_CMD_INIT( diag, EVENT_DIAG),
    CORE_CMD_INIT( ip, EVENT_IP),
    CORE_CMD_INIT( link, EVENT_LINK),
    CORE_CMD_INIT( com, EVENT_COM),
    CORE_CMD_INIT( occ, EVENT_OCC),
    CORE_CMD_INIT( pa, EVENT_PA),
    CORE_CMD_INIT( blsn, EVENT_BLSN),
    CORE_CMD_INIT( ppt, EVENT_PPT),
//	CORE_CMD_INIT( max, MAX_EVENT),
};

static void hand_none(struct dcore *core, union event_info *va)
{
//	dout( "%s: call!, time = %d\r\n", __func__, ptime() );

    return;
}
static void hand_diag(struct dcore *core, union event_info *va)
{
    dout( "%s: diag event! -> ignore\n", __func__ );

    return;
}

static void hand_ip(struct dcore *core, union event_info *va)
{

    dout( "%s: ip dial! -> ignore\n", __func__ );

    return;
}

/* hand com event */
static void hand_com(struct dcore *core, union event_info *va)
{
    u8_t type;
    u8_t id;
    u8_t lv;

    if(VCOM_TYPE_PCOM != va->com.type && VCOM_TYPE_DCOM != va->com.type )
        return;

    type = (VCOM_TYPE_PCOM == va->com.type) ? COM_IS_P: COM_IS_D;
    id = va->com.id;
    lv = (VDCOM_ID_UIC_RISING==id) ? 1: 0;

    switch( id )
    {
    case VCOM_ID_BUT:
        com_but(&(core->stat), &(core->com), type);
        break;

    case VDCOM_ID_UIC_RISING:	// lv -> 1
    case VDCOM_ID_UIC_FALLING:	// lv -> 0
        com_uic_in(&(core->com), lv);
        break;

    // network event hand
    case VCOM_ID_REQ_CON:
    case VCOM_ID_REQ_UNCON:
    case VCOM_ID_REP_SETUP:
    case VCOM_ID_REP_HANGUP:
        com_net(&(core->stat), &(core->com),
                &(va->com.info), type, id );
        break;

    default:
        dout("%s: switch default! -> ignore\r\n", __func__ );
        break;
    }

    return;
}

/* hand occ event */
static void hand_occ(struct dcore *core, union event_info *va)
{
    if(VOCC_START == va->occ)
    {
        occ_op(&(core->stat), 1 );
    }
    else if(VOCC_STOP == va->occ)
    {
        occ_op(&(core->stat), 0 );
    }

    return;
}

/* hand pa event */
static void hand_pa(struct dcore *core, union event_info *va)
{
    if(VPA_IN_BUT == va->pa)
    {
        pa_op(&(core->stat), &(core->pa), 1 );
    }
    else if(VPA_OUT_BUT == va->pa)
    {
        pa_op(&(core->stat), &(core->pa), 2 );
    }
    else if(VPA_UIC_IDLE == va->pa)
    {
        pa_op(&(core->stat), &(core->pa), 3 );
    }

    return;
}

/* hand ppt event */
static void hand_ppt( struct dcore *core, union event_info *va )
{
    if(VPPT_PUSHED == va->ppt)
    {
        // do ppt pushed
        ppt_op( &(core->stat), 1);
    }
    else if(VPPT_POPED == va->ppt)
    {
        // do ppt poped
        ppt_op( &(core->stat), 2);
    }

    return;
}

/* hand broadcast listen event */
static void hand_blsn(struct dcore *core, union event_info *va)
{
    if(VBLSN_START == va->blsn)
    {
        blsn_op( &(core->stat), 1);
    }
    else if(VBLSN_STOP == va->blsn)
    {
        blsn_op( &(core->stat), 2);
    }

    return;
}

/* hand cab link event */
static void hand_link(struct dcore *core, union event_info *va)
{
    // cab link information
    if( VLINK_OFF == va->link )
        core->stat.link = LINK_ST_OFF;
    else if( VLINK_ON == va->link)
        core->stat.link = LINK_ST_ON;

    return;
}

/* init dcore stat */
static void stat_init( struct dstat *st )
{
    if( 0 == st )
        return;

    // com part
    st->com_is = COM_IS_NONE;
    st->com_st = COM_ST_IDLE;

    // olsn part
    st->olsn = OLSN_ST_IDLE;

    // blsn part
    st->blsn = BLSN_ST_IDLE;

    // pa part
    st->pa_is = PA_IS_NONE;
    st->pa_st = PA_ST_IDLE;

    // link part
    st->link = LINK_ST_OFF;
	
//	dump_stat(st);

    return;
}


/* init all app module */
void dcore_init(struct dcore *core)
{
    if( 0 == core )
        return;

    // init all stat
    stat_init( &(core->stat) );

    // init com part
    com_init( &(core->com) );

    // init pa part
    pa_init( &(core->pa) );

    // init loud part
    dloud_init();

    // mic init part
    mic_init();

    // refer to debug
    dnet_debug_setup();

    // refer to multicast
    dnet_multi_setup();
	
//	dump_com( &core->com);
	
    return;
}

// check all timeout && uic check && led blink ....
static void dcore_check( struct dcore *core )
{
    if( 0 == core )
        return;

    com_check( &(core->com), &(core->stat) );

    pa_check(&(core->pa), &(core->stat) );


    return;
}

/* hand all event */
void dcore_hand(struct dcore *core, u8_t type, union event_info *value)
{
    if(0 == core || type >= MAX_EVENT || 0 == value)
        return;

//	dout( "%s: call !\r\n", __func__ );

    // call callback function, according to type
    if( NULL != cmd_table[type].cb )
        cmd_table[type].cb(core, value);

// check all timeout && uic check && led blink ....
    dcore_check(core);

    return;
}


/************* dump dcore for debug **************/
/* only for debug */
void dump_dcore(struct dcore *core)
{
    if( 0 == core )
        return;

    return;
}

