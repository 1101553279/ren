#ifndef __DCORE_H__
#define __DCORE_H__

#include "btype.h"
#include "blist.h"
#include "loud.h"
#include "com.h"
#include "pa.h"
#include "dnet_multi.h"
#include "event.h"
#include "dcore_hand.h"

struct dcore {
    /* 将代码中相同，关联的部分抽象出来 */
    u8_t self_addr;
    struct dstat stat;	// pcom / dcom
    // occ listen
    // pa in/out
    // broadcast listen
    struct com com;		// com structure
    struct pa pa;		// pa structure
//    struct loud loud;		// all functions refer to loud
//    struct net_pcb multi;	// multicast network info
//    struct net_pcb debug;	// debug network info
};

//
void dcore_init(struct dcore *core);
void dcore_hand(struct dcore *core, u8_t type, union event_info *value);
void dump_dcore(struct dcore *core);

#endif
