#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
//
#include "libev/ev.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "udpport.h"
#include "mtp.h"


#define     PORT 6666


struct SockCmd {

   // int fd;
    char *ip;
    //uint16_t port;
    int port;
};



int main(int argc, char *argv[])
{
    int i;
    printf("This is a test of udp port demo.\n");
    Sockcmd cmd;

    i = setUdpParam(&cmd);
    if (i < 0)
    {
        printf(" Oo test fail!\n");
    }

    return 0;
}

int setUdpParam(Sockcmd *cmd)
{

	//memset(cmd->ip,0,sizeof(cmd->ip));
 	cmd->ip = 0;
	cmd->port = 8888;
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	//addr = (struct sockaddr_in *)PORT;
	
	int fd;
	fd = initudp(cmd,&addr);
	if(fd < 0)
	{
		printf("initudp fail\n");
		return -1;
	}
	
	return 0;
}

 int initudp(Sockcmd *cmd,struct sockaddr_in *addr)
 {
	 struct ev_loop *loop = ev_default_loop(0);
	 //创建一个io watcher和一个timer watcher  
     struct ev_io socket_watcher;  
   //  struct ev_timer timeout_w; 
	
	 memset(addr, 0, sizeof(*addr));
	 
	 int fd = sock_udp(cmd->ip, cmd->port, addr);
	 if(fd == -1)
     {
		 printf("create udp socket fail:%s, %d:%s\n",cmd->ip,cmd->port,strerror(errno));
		 return -1;
     }
	 else
     {
		 printf("create udp socket success. ip: %s port: %d SOCKET: %d\n",cmd->ip,cmd->port,fd);
		 
		 ev_io_init(&socket_watcher, udp_rw_cb, fd, EV_READ);  
		 ev_io_start(loop, &socket_watcher);  
      
		 //发送心跳包  
		 //ev_timer_init(&timeout_w, timer_beat, 1, 0);  
		 //ev_timer_start(loop, &timeout_w);  
      
		 ev_run(loop, 0);
	 }
	 
	 return 0;
 }

int sock_udp(char *ip, int port,struct sockaddr_in *addr)
{
	int fd;
	
	//struct sockaddr_in addr;

	fd = socket(AF_INET,SOCK_DGRAM,0); //0 or IPPROTO_UDP
	if(fd < 0){
		perror("socket init error!\n");
		return -1;
	}
	
	do
	{
		if(!set_reuseaddr(fd))
		{
			perror("Set reuseaddr error!\n");
			break;
		}
		if(!set_nonblock(fd))
		{
			perror("Set nonblck error!\n");
			break;
		}
		memset(addr,0,sizeof(*addr));
		addr->sin_family = AF_INET;
		addr->sin_port = htons(port);
		if(ip ==0 || *ip == '\0')
		{
			addr->sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else
		{
			addr->sin_addr.s_addr = inet_addr(ip);
		}
		
		if (bind(fd, (struct sockaddr *)addr, sizeof(*addr)) == -1)
		{
            printf("bind scoket failed!\n");
			break;
		}  
	
		return fd;
		
	}while(0);
	
	close(fd);
	return -1;	
	
}

bool set_reuseaddr(int fd)
{
	int on = 1;
	if(setsockopt(fd,SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
	{
		return false;
	}
	return true;
}

bool set_nonblock(int fd)
{
	int flags = fcntl(fd,F_GETFL,0);
	if (flags == -1)
	{
		return false;
	}
	
	if (fcntl(fd,F_SETFL,flags | O_NONBLOCK) == -1)
	{
		return false;
	}
	return true;
}


void udp_rw_cb(struct ev_loop *loop, struct ev_io *w, int events)
{
	do
	{
		char buf[4096];
		int length = sizeof(buf);
		int PendLen_ = 0;  //********
		Frame *frame = 0;
	
		if (events & EV_ERROR)
		{
			printf("error event in read\n");
			return;
		}
		if (!handleUdpRecv(w->fd,buf,&length,loop))
		{
			printf("Recv udp fail : %s SOCKET:%d\n",strerror(errno),w->fd);
			break;
		}
		buf[length] = '\0';
		printf("Recv buf data:%s\n",buf);
		//MTP处理
		Frame *p = (Frame *)&buf;
		try
		{
			frame = new Frame;
			memcpy(frame,p,p->hdr.len);
			
			if (mtp_receive(frame->hdr.e1,frame->hdr.ts,frame->hdr.len - FRAME_HEADER_LENGTH) < 0)
				TRACE("mtp_receive\n");
		}
		catch (bad_alloc &e)
		{
			e;
			return -2;
		}
		
#if 0
		if (handData((const char *)buf,&length) < 0)
		{
			printf("接收数据有误！\n");
			return;
		}
#endif

		if (events & EV_WRITE)  //
 		{
			if(!handleUdpSend())
			{
				break;
			}
		}
		
		return;
	
	}while (0);
}

bool handleUdpSend(void)  //后续
{
	return false;
}

bool handleUdpRecv(int fd, char *buf, int *length,struct ev_loop *loop)
{
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	
	int ret = recvfrom(fd, buf, *length, 0, (struct sockaddr *)&from, &fromlen);
	if (ret == -1)
	{
		return false;
	}
	
	if (ret == 0)
	{
		printf("client disconnected.\n");
		freelibev(loop,fd);
		return false;
	}
	
	/*if(libevlist[ret] == NULL)
	{
		printf(" recvfrom error :ret is [%d]\n",ret);
		return false;
	}
	*/
	*length = ret;
	return true;
}

void timer_beat(struct ev_loop *loop, struct ev_timer *w, int events)
{
	float timeout = 2.0;
	printf("Send beat per[%f]\n",timeout);
	//fflush(stdout);
	
	if (EV_ERROR & events)
	{
		printf("error event in timer_beat\n");
		return;
	}
	
	ev_timer_set(w,timeout,0);
	ev_timer_start(loop,w);
	return;
}

int freelibev(struct ev_loop *loop, int fd)
{
	if(libevlist[fd] == NULL)
	{
		printf("the fd already freed [%d]\n",fd);
		return -1;
	}
	
	close(fd);
	ev_io_stop(loop,libevlist[fd]);
	free(libevlist[fd]);
	libevlist[fd] = NULL;
	return 1;
}

int handData(const char *data,int *len)
{
	return 0;
}