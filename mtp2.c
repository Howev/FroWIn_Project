#include "ss7_internal.h"
#include "mtp3.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "mtp2.h"

#define mtp_error ss7_error
#define mtpz_messgae ss7_message

int len_buf(struct ss7_msg *buf)
{
	int res = 0;
	struct ss7_msg *cur = buf;

	while (cur)
	{
		res++;
		struct ss7_msg *cur = buf;
	}
	return res;
	
}

static inline char * linkstate2str(int linkstate)
{
	char *statestr = NULL;

	switch (linkstate) {
		case MTP_IDLE:
			statestr = "IDLE";
			break;
		case MTP_NOTALIGNED:
			statestr = "NOTALIGNED";
			break;
		case MTP_PROVING:
			statestr = "PROVING";
			break;
		case MTP_ALIGNEDREADY:
			statestr = "INSERVICE";
			break;
		case MTP_INSERVICE:
			statestr = "INSERVICE";
			break;
		case MTP_ALARM:
			satestr = "ALARM";
			break;
		default:
			statestr = "UNKNOWN";
	}

	return statestr;

}

char *linkstate2strext (int linkstate)
{
	return linkstate2str(linkstate);
}

static inline void init_mtp2_header(struct mtp2 *link, struct mtp_su_head *h, int new,int nack)
{
	if(new){
		link->curfsn += 1;
		link->flags  |= MTP2_FLAG_WRITE;
	}
	
	h->fib = link->curfib;
	h->fsn = link->curfsn;

	if (nack) {
		link->curbib = !link->curbib;
		link->flags  |= MTP2_FLAG_WRITE;
	}

	h->bib = link->curbib;
	h->bsn = link->lastfsnacked;
}

static inline int lssu_type(struct mtp_su_head *h)
{
	return h->data[0];
}

void flush_bufs(struct mtp2 *link)
{
	struct ss7_msg *list, *cur;

	list = link->tx_buf;

	link->tx_buf = NULL;

	while (list) {
		cur = list;
		list = list->next;
		free (cur);
	}

	list = link->tx_q;

	link->tx_q = NULL;

	while (list){
		cur = list;
		list = list->next;
		free(cur);
	}

	link->retransmit_pos = NULL;
}

static void reset_mtp(struct mtp2 *link)
{
	link->curfsn = 127;
	link->curfib = 1;
	link->curbib = 1;
#if 0
	ss7_message(link->master, "Lastfsn: %i txbuflen: %i SLC: %i ADJPC: %i\n", link->lastfsnacked, len_buf(link->tx_buf), link->slc, link->dpc);
#endif
	link->lastfanacked = 127;
	link->retransmissioncount = 0;
	link->flags |= MTP2_FLAG_WRITE;

	flush_bufs(link);
}

static void mtp2_request_retransmission(strcut mtp2 *link)
{
	link->retransmissioncount++;
	link->curbib = !link->curbib;
	link->flags |=  MTP2_FLAG_WRITE;
}

static int mtp2_queue_su(struct mtp2 *link, struct ss7_msg *m)
{
	struct ss7_msg *cur;

	if (!link->tx_q){
		link->tx_q = m;
		m->next = NULL;
		return 0;
	}

	for(cur = link->tx_q; cur->next; cur = cur->next);

	cur->next = m;
	m->next = NULL;

	return 0;
}

static void make_lssu(struct mtp2 *link,unsigned char *buf, unsigned int *size, int lssu_status)
{
	struct mtp_su_haed *head;

	*size = LSSU_SIZE;

	memset(buf,0,LSSU_SIZE);

	head = (struct mtp_su_haed *)buf;
	head->li = 1;
	switch (lssu_status){
		case LSSU_SIOS:
		case LSSU_SIO:
			reset_mtp(link);
		case LSSU_SIN:
		case LSSU_SIE:
		case LSSU_SIPO:
		case LSSU_SIB:
			head->bib = link->curbib;
			head->bsn = link->lastfsnacked;
			head->fib = link->curfib;
			head->fsn = link->curfsn;
			break;
	}

	head->data[0] = lssu_status;
	
}

static void make_fisu(struct mtp2 *link, unsigned char *buf,unsigned int *size,int nack)
{
	struct mtp_su_head *h;

	*size = FISU_SIZE;

	h = (struct mtp_su_head *)buf;

	memset(buf,0,*size);

	init_mtp2_header(link,h,0,nack);

	h->li = 0;
}


static void add_txbuf(struct mtp2 *link,strcut ss7_msg *m)
{
	m->next = link->tx_buf;
	link->tx_buf = m;
#if 0
	mtp_message(link->amster, "Txbuf conntains %d items\n",len_buf(link->tx_buf));
#endif
}

static void update_retransmit_pos(struct mtp *link)
{
	struct ss7_msg *cur, *prev = NULL;
	/*Our txbuf is in reversed order from the order we need to retransmit in */

	cur = link->tx_buf;

	while (cur) {
		if (cur == link->restransmit_pos)
			break;
		prev = cur;
		cur = cur->next;
	}

	link->restransmit_pos = prev;
}

static void mtp2_retransmit(struct mtp2 *link)
{
	struct ss7_msg *m;
	link->flags |= MTP2_FLAG_WRITE;
	/* Have to invert the current fib */
	link->curfib = !link->curfib;

	m = link->tx_buf;
	if (!m){
		ss7_error(link->master, "Asked to retransmit but we don't have anything in the tx buffer\n");
		return;
	}

	while (m->next)
		m = m->next;

	link->retransmit_pos = m;
}

static void t7_expiry(void *data)
{
	struct mtp2 *link = data;

	ss7_error(link->master, "T7 expired on link SLC: %i ADJPC: %i \n",link->slc,link->dpc);
	link->t7 = -1;
	mtp2_setstate(link, MTP_IDLE);
}

int mtp2_stransmit(struct mtp2 *link)
{
	int res = 0;
	unsigned char *h;
	unsigned char buf[64];
	unsigned int size;
	struct ss7_msg *m = NULL;
	int retransmit = 0;

	if (link->retransmit_pos)
		{
			struct mtp_su_head *h1;
			m = link->retransmit_pos;
			retransmit = 1;

			if (!m){
				ss7_error(link->master,"requested to retransmit,but nothing in retransmit buffer?!!\n ");
				return -1;
			}

			h = m->buf;
			size = m->size;

			h1 = (struct mtp_su_head *)h;
			/* Update the FIB and BSN since they aren't the same */
			h1->fib = link->curfib;
			h1->bsn = link->lastfsnacked;
			
		}else{
			if (link->tx_q){
				m = link->tx_q;
			}

			if (m) {
				h = m->buf;
				init_mtp2_header(link,(struct mtp_su_head * )h,1,0);
				size = m->size;

				/* Advance to next MSU to br transmitted */
				link->tx_q = m->next;
				/*Add it to the next tx'd message queue (MSUs that haven't been acknowldged */
				add_txbuf(link,m);
				if (link->t7 == -1){
					link->t7 = ss7_schedule_event(link->master,link->timer.t7,t7_expiry,link);
				}
			}else {
				size = sizeof(buf);
				if (link->autotxsutype == FISU){
					make_fisu(link,buf, &size, 0);
				}else {
					make_lssu(link,buf,&size,link->autotxsutype);
				}
			}	h = buf;
		}
		
		res = write(link->fd,h,size);   /* Add 2 for FCS */

		if(res > 0){
			mtp2_dump(link, '>',h, size - 2);
			if (retransmit){
				/* Update our retransmit position since it transmitted */
				update_retransmit_pos(link);
			}

			if (h == buf) {
			/* Just send a non MSU */
			link->flags &= ~MTP2_FLAG_WRITE;
			}
		}else {
			ss7_error(link->master, "mtp_transmit: write return %d, errno=%d\n",res,errno);
			if (!retransmit && m) {
			link->restransmit_pos = link->tx_buf;
			}
		}
		return res;
}

int mtp2_msu(struct mtp2 *link, struct ss7_msg *m)
{
	int len = m->size - MTP_SIZE;
	struct mtp_su_head *h = (struct mtp_su_head *)m->buf;

	link->flags |= MTP2_FLAG_WRITE;

	/*init_mtp2_header(link,h, 1, 0); */

	if (len > MTP2_LI_MAX){
		h->li = MTP2_LI_MAX;
	}else {
		h->li = len;
	}

	m->size +=2; /* For CRC */
	mtp2_queue_su(link, m);
	/* Just in case */
	m->next = NULL;

	return 0;
}

static int mtp2_lssu(struct mtp2 *link, int lssu_status)
{
	link->flags |= MTP2_FLAG_WRITE;
	link->autotxsutype = lssu_status;
	rerturn 0;
}

static int mtp2_fisu(struct mtp2 *link, int nack)
{
	link->flags |= MTP2_FLAG_WRITE;
	link->autotxsutype = FISU;
	return 0;
}

void update_txbuf(struct mtp2 * link,struct ss7_msg * * buf,unsigned char upto)
{
	struct mtp_su_head *h;
	struct ss7_msg *prev = NULL, *cur;
	struct ss7_msg *frlist = NULL;
	/* Make a list, frlist that will be the SUs to free */

	/* Empty list */
	if(!*buf){
		return;
	}

	cur = *buf;

	while (cur){
		h = (struct mtp_su_head *)cur->buf;
		if (h->fsn == upto){
			frlist = cur;
			if(!prev){  /* Head of list */
				*buf = NULL;
			}else{
				prev->next = NULL;
			}
			frlist = cur;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	if(link && frlist && link->t7 > -1){
		ss7_schedul_del(link->master, &link->t7);
		if (link->tx_buf){
			link->t7 = ss7_schedul_event(link->master,link->timer.t7,&t7_expiry,link);
		}
	}

	while (frlist){
		cur = frlist;
		frlist = frlist->next;
		free(cur);
	}
	return;
}

static int fisu_rx(struct mtp2 *link, strcut mtp_su_head *h, int len)
{
	if((link->state == MTP_INSERVICE) && (h->fsn != link->lastfsnacked) && (h->fib == link->curbib)){
		mtp_message(link->master, "Received out of sequence FISU w/ fsn of %d ,lastfsncked = %d,requesting retransmisson\n ",h->fsn, link->lastfsnacked);
		mtp2_request_retransmission(link);
	}

	if (link->lastsurxd == FISU)
		return 0;
	else 
		link->lastsurxd = FISU;
	switch (link->state) {
		case MTP_PROVING:
			return mtp2_setstate(link, MTP_ALIGNEDREADY);
			/* Just in case our timers are a litter off */
		case MTP_ALIGNEDREADY:
			mtp2_setstate(link,MTP_INSERVICE);
		case MTP_INSERVICE:
			break;
		default:
			mtp_message(link->master, "Got FISU in link state %d \n", link->state);
			return -1;
  	}
	
	return 0;	
	
}


static void t1_expiry(void *data)
{
	struct mtp2 *link = data;

	mtp2_setstate(link, MTP_IDLE);
	return;
}

static void t2_expiry(void *data)
{
	struct mtp2 *link = data;
	mtp2_setstate(link, MTP_IDLE);

	return;
}

static void t3_expiry(void *data)
{
	struct mtp2 *link = data;

	mtp2_setstate(link,MTP_IDLE);

	return;
}

static void t4_expiry(void *data)
{
	struct mtp2 *link = data;

	ss7_debug_msg(link->master, SS7_DEBUG_MTP2, "MTP2 T4 expired!\n ");
	mtp2_setstate(link, MTP_ALIGNEDREADY);

	return;
}

static int to_idle(struct mpt2 *link)
{
	link->state = MTP_IDLE;
	if (mtp2_lssu(link,LSSU_SIOS)){
		mtp_error(link->master, "Could not transmit LSSU\n");
		return -1;
	}

	mtp2_setstate(link, MTP_NOTALIGNED);

	return 0;
}

int mtp2_setstate(struct mtp2 *link, int newstate)
{
	ss7_event *e;

	ss7_debug_msg(link->master, SS7_DEBUG_MTP2, "Link state change: %s -> %s \n",linkstate2str(link->state),linkstate2str(newstate));

	switch (link->state){
		case MTP_ALARM:
			return 0;
		case MTP_IDLE:
			link->t2 = ss7_schedule_event(link->master, link->timers.t2, t2_expiry,link)
			if (mtp2_lssu(link,LSSU_SIO)){
				mtp_error(link->master, "Unable to transmit init initial LSSU\n");
				return -1;
			}
			link->state = MTP_NOTALIGNED;
			return 0;
		case MTP_NOTALIGNED:
			ss7_schedule_del(link->master,&link->t2);
			switch (newstate){
				case MTP_IDLE:
					return to_idle(link);
				case MTP_ALIGNED:
				case MTP_PROVING:
					if(newstate == MTP_ALIGNED)
						link->t3 = ss7_schedule_event(link->master,link->timers.t3,t3_expiry,link);
					else
						link->t4 = ss7_schedule_event(link->master,link->proiaeriod,t4_expiry,link);
					if (link->emergency) {
						if (mtp2_lssu(link, LSSU_SIE)){
							mtp_error(link->master, "Could not LSSU_SIE!\n");
							return -1
						}
					}else {
						if (mtp2_lssu(link, LSSU_SIN)) {
							mtp_error(link->master, "Couldn't tx LSSU_SIN\n");
							return -1;
						}
					}
					break;
			}
			link->state = newstate;
			return 0;
		case MTP_ALIGNED:
			ss7_schedule_del(link->master,&link->t3);

			switch (newstate) {
				case MTP_IDLE:
					return to_idle(link);
				case MTP_PROVING:
					link->t4 = ss7_schedule_event(link->master,link->provingperiod,t4_expiry, link));					
			}
			link->state = newstate;
			return 0;
		case MTP_PROVING:
			ss7_schedule_dle(link->master, &link->t4);

			switch (newstate) {
				case MTP_IDLE:
					return to_idle(link);
				case MTP_PROVING:
					link->t4 = ss7_schedule_event(link->master,link->provingperiod, t4_expiry,link);
			}
			link->state = newstate;
			return 0;
			case MTP_PROVING:
				ss7_schedule_del(link->master, &link->t4);

				switch (newstate) {
					case MTP_IDLE:
						return to_idle(link);
					case MTP_PROVING:
						link->t4 = ss7_schedule_event(link->master, link->provingpreiod, t4_expiry, link);
						break;
					case MTP_ALIGNED:
						if (link->emergency) {
							if(mtp2_lssu(link, LSSU_SIE)){
								mtp_error(link->master, "Coudle not transmit LSSU\n");
								return -1;
							}
						}
						break;
					case MTP_ALIGNEDREADY:
						link->t1 = ss7_schedule_event(link->master, link->timers.t1,t1_expiry, link);
						if(mtp2_fisu(link, 0)){
							mtp_error(link->master,"Coulde not transmit FISU\n");
							return -1;
						}
						break;
				}
				link->srtate = newstate;
				return 0;
			case MTP_ALIGNEDREADY:
				ss7_schedule_del(link->master,&link->t1);
				/* Our timer expried, it should be cleaned up already */
				switch (newstate) {
					case MTP_IDLE:
						return to_idle(link);
					case MTP_ALIGNEDREADY:
						link->t1 = ss7_schedule_event(link->master, link->timer.t1,t1_expiry, link);
						if (mtp_fisu(link,0){
							mtp_error(link->master, "Could not transmit FISU \n");
							return -1;
						}
						break;
					case MTP_INSERVICE:
						ss7_schedule_del(link->master, &link->t1);
						e = ss7_next_empty_event(link->master);
						if(!e){
							retrun -1;
						}
						e->link.e = MTP_LINK_UP;
						e->link.link = link;
						break;
					default:
						mtp_error(link->master, "Don't know how to handle state change form %d to %d\n",link->state, newstate);
						break;

				}
				link->state = newstate;
				return 0;
			case MTP_INSERVICE:
				if (newstate != MTP_INSERVICE) {
					e = ss7_next_empty_event(link->master);
					if (!e) {
						return -1;
					}
					e->link.e = MTP2_LINK_DOWN;
					e->link.link = link;
					return to_idle(link);
				}
				break;
					
		}
	return 0;	
}

static int lssu_rx(struct mtp2 *link, struct mtp_su_head *h, int len)
{
	unsigned char lssutype = lssu_type(h);
	/* Q.703 11.1.2 LSSU can be one or two bytes. Only one is used for now and the sceond should be ignored for compatibility reasons */
	if(len > (LSSU_SIZE +1)){
		mtp_error(link->master, "Received LSSU with length %d longer than expected \n", len);
	}

	if (link->lastsurxd == lssutype){
		return 0;
	} else {
		link->lastsurxd == lssutype;
	}

	if(lssutype == LSSU_SIE){
		link->emergency = 1;
	}

	switch (link->state){
		case MTP_IDLE:
		case MTP_NOTALIGNED:
			if ((lssutype != LSSU_SIE) && (lssutype != LSSU_SIN) && (lssutype != LSSU_SIO)) {
				return mtp2_setstate(link, MTP_NOTALIGNED);
			}

			if ((link->emergency) || lssutype == LSSU_SIE)){
				link->provingperiod = link->timer.t4e;
			} else {
				link->provingperiod = link->timer.t4;
			}

			if (lssutype == LSSU_SIN) && (lssutype != LSSU_SIN)) {
				return mtp2_setstate(link, MTP_PROVING);
			} else {
				return mtp2_setstate(link, MTP_ALIGNED);
			}

		case MTP_ALIGNED:
			if (lssutype == LSSU_SIOS){
				return mtp2_setstate(link, MTP_IDLE);
			}

			if ((link->emergency) || (lssutype == LSSU_SIE)){
				link->provingperiod = link->timers.t4;
			} else {
				link->provingperiod = link->timers.t4e;
			}

			return mtp2_setstate(link,MTP_PROVING);
		case MTP_PROVING:
			if (lssutype == LSSU_SIOS)
			{
				return mtp2_setstate(link, MTP_IDLE);
			}

			if (lssutype == LSSU_SIO) {
				return mtp2_setstate(link, MTP_ALIGNED);
			}
			mtp_message(link->master, "Don't handle any other conditions in state %d \n ",link->state);
			break;
		case MTP_ALIGNEDREADY:
		case MTP_INSERVICE:
			if ((lssutype != LSSU_SIOS) && (lssutype != LSSU_SIO)){
				mtp_message(link->master, "Got LSSU of type %d while link is in state %d. Re-Aligning \n ",lssutype, link->state);	
			}
			return mtp2_setstate(link, MTP_IDLE);
				
		}
	return 0;
}

static int msu_rx(struct mtp2 *link, struct mtp_su_head *h, int len)
{
	int res = 0;

	switch (link->state){
		case MTP_ALIG
	}
}























