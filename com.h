#ifndef __COM_H__
#define __COM_H__
#include "btype.h"
#include "blist.h"
#include "led.h"
#include "dstat.h"

enum{
	UIC78_OUT_NONE,
	UIC78_OUT_CON,
	UIC78_OUT_SETUP,
	UIC78_OUT_HANGUP,
};

enum{
	UIC78_IN_NONE,
	UIC78_IN_CON,
	UIC78_IN_SETUP,
	UIC78_IN_HANGUP,
};

struct uic78{
	u8_t flag;		// idle/input/output(0 ~ 2)
// output
	u8_t ocur;		// output level
	u8_t onu;		// output number
	u32_t otime;	// last output time
// input
	u8_t irising;	// rising edge number
	u8_t ifalling;	// falling edge number
	u32_t itime;	// record start time
};

struct com_info {
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

struct com_node {
    u8_t use;				//1 -> in use, 0 -> not use
    struct list_head head;	// for com list
    struct com_info node;	// com data
};

struct dcom_wait {
    u8_t flag;			// 0 -> idle
						// 1 -> self call
						// 2 -> other wait
    u32_t time;			// call start time
//    struct com_info info;
	struct list_head dq;
};

struct com {
	struct uic78 uic;
    struct com_info cinfo;	// com info save
	struct com_info tt;		// temporiary variable
    struct led pled;		// pcom led tips
    struct led dled;		// dcom led tips

    struct list_head pq;	// pecu wait
    // dacu <- pecu

//    struct list_head dq;	// other dacu wait
//    // dacu <- dacu

    struct dcom_wait sd;	// dacu waitting ( self call & other call )
    // dacu <-> dacu
};

typedef u8_t (*node_find_cb_t)(struct com_info *dst, struct com_info *src);

void com_init(struct com *com);
u8_t uic78_idling( struct uic78 *uic);
u8_t uic78_inputing( struct uic78 *uic);
u8_t uic78_outputing( struct uic78 *uic);
void uic78_intput_set(struct uic78 *uic, u8_t type);
u8_t uic78_input_check(struct uic78 *uic);
void uic78_output_set(struct uic78 *uic, u8_t cmd);
void uic78_output_check(struct uic78 *uic);

u8_t com_idling(struct com *com, u8_t type);
u8_t com_waitting(struct com *com, u8_t type);
u8_t com_calling(struct com *com, u8_t type);
void to_dcom_idle(struct dcom_wait *dw);
void to_dcom_wait(struct dcom_wait *dw);
void to_dcom_call(struct dcom_wait *dw);
u8_t com_call_timeout(struct dcom_wait *dw);

struct led *com_led_is(struct com *com, u8_t type);
struct list_head *com_list_is(struct com *com, u8_t type);


void com_info_init( struct com_info *ci );
u8_t rd_info_cb(struct com_info *dst, struct com_info *src);
u8_t sd_info_cb(struct com_info *dst, struct com_info *src);
struct com_node* com_node_find(struct list_head *head, struct com_info *cmp, node_find_cb_t cb );
void com_node_add(struct list_head *head, struct com_info *tt );
void com_node_del(struct list_head *head, struct com_node *cn );


// only for debug
void dump_com_uic(struct uic78 *uic);
void dump_com_info_title(char *name);
void dump_com_info_data(struct com_info *ci, u8_t no);
void dump_com_info(struct com_info *ci, char *name);
void dump_list(struct list_head *head, char *name);
void dump_com_wait(struct dcom_wait *dw, char *name );
void dump_com(struct com *cm );

void dump_node_base(void);
#endif
