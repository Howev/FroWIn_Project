#ifndef _SS7_MTP_H
#define _SS7_MTP_H

#include "ss7_internal.h"

#define MTP2_LI_MAX 		63

#define SIF_MAX_SIZE 		272

#define MTP2_SU_HEAD_SIZE 	3
#define MTP2_SIZE			MTP2_SU_HEAD_SIZE

/*MTP2 Timers */
/*For ITU 64kbps links */
#define ITU_TIMER_T1 			45000
#define ITU_TIMER_T2 			50000
#define ITU_TIMER_T3			1500
#define ITU_TIMER_T4_NORMAL		8500
#define ITU_TIMER_T4_EMERGENCY	500
#define ITU_TIMER_T7			1250

/*For ANSI links */
#define ANSI_TIMER_T1			16000
#define ANSI_TIMER_T2			11500
#define ANSI_TIMER_T3			11500
#define ANSI_TIMER_T4_NORMAL	2300
#define ANSI_TIMER_T4_EMERGENCY	600
#define ANSI_TIMER_T7			1250

/* Bottom 3 bits in LSSU status field */
#define LSSU_SIO 	0	/* Out of alignment */
#define LSSU_SIN	1	/* Normal alignment */
#define LSSU_SIE 	2	/* Emergency alignemt */
#define LSSU_SIOS 	3	/* Out of Service */
#define LSSU_SIPO	4	/* MTP2 cannot reach MTP3, usless for us  */
#define LSSU_SIB	5	/* MTP2 congestion */

#define FISU		6

/* More MTP2 definitations */
/* Various sizes */
#define MTP_MAX_SIZE 	277  /* 276 + 1 for RSIS */
#define LSSU_SIZE 		6
#define LSSU_SIZE		5

/* MTP2 Link states */
#define MTP_IDLE			0
#define MTP_NOTALIGNED		1
#define MTP_ALIGNED			2
#define MTP_PROVING			3
#define MTP_ALIGNEDREADY 	4
#define MTP_INSERVICE		5
#define MTP_ALARM			6


struct mtp_su_haed 
{
	/* Common header for all signaling units */
	unsigned char bsn:7;
	unsigned char bib:1;
	unsigned char fsn:7;
	unsigned char fib:1;
	unsigned char li:6;
	unsigned char spare:2;
	unsigned char data[0];
}__attribute__((packed));

struct ss7;

struct mtp2_timers
{
	int t1;
	int t2;
	int t3;
	int t4;
	int t4e;
	int t7;
};

struct mtp2_timers{
	int state;
	int std_test_passed;
	int inhibit;
	int changeover;
	unsigned int got_sent_netsmg;	

	struct ss7_msg *co_cuf;
	struct ss7_msg *cb_cuf;

	unsigned char curfsn:7;
	unsigned char cutfib:1;
	unsigned char lastfsnacked:7;  /*store here before rset_mtp*/

	unsigned char curbib:1;
	int fd;
	int flags;

	int mtp3_timer[MTP_MAX_TIMERS];
	int q707_t1_failed;

	/* Timers */
	int t1;
	int t2;
	int t3; 
	int t4;
	int t7;
	struct mtp2_timers timers;

	int slc;
	int net_mng_sls;

	int emergency;
	int provingperiod;
	unsigned int dpc;

	int aotutxsutype;
	int lastsurxd;
	int lastsutxd;

	/*Lined ralated status */
	unsigned int retransmissioncount;

	struct ss7_msg *tx_buf;
	struct ss7_msg *tx_q;
	struct ss7_msg *retransmit_pos;
	struct ss7_msg *co_tx_buf;  /* store here before rset_mtp flush it */
	struct ss7_msg *co_tx_q;
	struct adjacent_sp *adj_sp;
	unsigned char cb-seq;
	struct ss7 *master;
};

/* Flags for the struct mtp2 flags parameter */
#define MTP2_FLAG_DAHDIMTP2 (1 << 0)
#define MTP2_FLAG_WRITE		(1 << 1)

/* Initialize MTP link */
int mtp2_start(struct mtp2 *link, int emergency);
int mtp2_stop(struct mtp2 *link);
int mtp2_alarm(struct mtp2 *link);
int mtp2_noalarm(struct mtp2 *link);
int mtp2_setstate(struct mtp2 *link,int state);
struct tmp2 * mtp2_new(int fd,unsigned int switchtype);
int mtp2_transmit(struct mtp2 *link);
int mtp2_receive(struct mtp2 *link, unsigned char *buf, int len);
int mtp2_msu(struct mtp2 *link, struct ss7_msg *m);
void  mtp2_dump(struct mtp2 *link, char prefix, unsigned char *buf, int len);
char *linkstate2strext(int linkstate);
void update_txbuf(struct mtp2 *link, struct ss7_msg **buf, unsigned char upto);
int len_buf(struct ss7_msg *buf);
void flush_bufs(struct mtp2 *link);

#endif







