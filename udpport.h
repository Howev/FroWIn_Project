
/* *  udpport.h  *  */

#ifndef _UDPPORT_H
#define _UDPPORT_H

#include "libev/ev.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "mtp.h"

#define MAX_ALLOWED_    128

const size_t BUF_SIZE = 1024;
const size_t PEND_BUF_SIZE = BUF_SIZE * 2;

#pragma pack(1)
struct FrameHdr
{
	unsigned short len;
	unsigned char e1;
	unsigned char ts;
};

struct FrameHdr
{
	FrameHdr hdr;
	unsigned char data[302];
};
#pragma pack()

const size_t FRAME_HEADER_LENGTH = sizeof(FrameHdr);
const size_t FRAME_LENGTH = sizeof(FrameHdr) + 302;

typedef struct SockCmd Sockcmd;

//void read_cb(struct ev_loop *loop,struct ev_io *watcher,int revents);

//typedef enum {false, true }bool;
	struct ev_io *libevlist[MAX_ALLOWED_] = {NULL};
	int setUdpParam(Sockcmd *cmd);
	int initudp(Sockcmd *cmd, struct sockaddr_in *addr);
	int sock_udp(char *ip, int port,struct sockaddr_in *addr);

	bool set_reuseaddr(int fd);
	bool set_nonblock(int fd);
	bool handleUdpRecv(int fd, char *buf, int *length, struct ev_loop *loop);
	bool handleUdpSend(void);

	void udp_rw_cb(struct ev_loop *loop,struct ev_io *w, int events);
	void timer_beat(struct ev_loop *loop, struct ev_timer *w, int events);
	int freelibev(struct ev_loop *loop, int fd);
	int handData(const char *data,int *len);


	char pendBuf_[PEND_BUF_SIZE];
	size_t pendLen_;
	


#endif /* _UDPPORT_H */

