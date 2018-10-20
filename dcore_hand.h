#ifndef __DCORE_HAND_H__
#define __DCORE_HAND_H__

#include "btype.h"
#include "event.h"
#include "dstat.h"

// dcore com event hand
void com_but( struct dstat *st, struct com *com, u8_t type);
void com_uic_in(struct com *com, u8_t type);
void com_net(struct dstat *st, struct com *com,
             struct com_info *in, u8_t type, u8_t id);
void com_check(struct com *com, struct dstat *stat);

// occ
void occ_op(struct dstat *stat, u8_t op);

// pa
void pa_op(struct dstat *stat, struct pa *pa, u8_t op);
void pa_check(struct pa *pa, struct dstat *stat);

// ppt op
void ppt_op(struct dstat *stat, u8_t op);

// broadcast listen
void blsn_op(struct dstat *stat, u8_t op);

#endif
