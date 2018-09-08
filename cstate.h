#ifndef _CSTATE_H_
#define _CSTATE_H_

#include <stdlib.h>
#include "mysocket.h"

enum c_state
{
	CS_START,
	CS_SOCKS_METHOD = 0,
	CS_SOCKS_REQUEST,
	CS_SOCKS_REQUEST_INPROGESS,
	CS_DATA,
};
#define C_BUFLEN	(16*1024)
#define M_BUFLEN	(512)

typedef struct conn_state
{
	ev_io w;
	enum c_state state;
	ev_io *pw;
	uint16_t rlen;
	uint16_t ridx;
	uint16_t wlen;
	uint16_t widx;
	uint8_t rbuf[C_BUFLEN];
	uint8_t wbuf[M_BUFLEN];
	uint8_t server[M_BUFLEN];
	uint8_t local[M_BUFLEN];
} cstate;

#define R_FREE(c)	(C_BUFLEN - c->ridx - c->rlen)
#define R_NEXT(c)	(c->rbuf + c->ridx + c->rlen)
#define R_IDX(c)	(c->rbuf + c->ridx)

typedef struct write_state
{
	ev_io w;
	cstate *c;
} wstate;

static inline cstate *new_cstate(void)
{
	cstate *c = malloc(sizeof(cstate));
	if (!c)
	{
		printf("CSTATE: oom\n");
		exit(1);
	}
	c->state = CS_START;
	c->pw = NULL;
	c->rlen = c->ridx = 0;
	c->wlen = c->widx = 0;
	return c;
}
static inline void del_cstate(cstate *c)
{
	free(c);
}
static inline wstate *new_wstate(void)
{
	wstate *w = malloc(sizeof(wstate));
	if (!w)
	{
		printf("WSTATE: oom\n");
		exit(1);
	}
	return w;
}
static inline void del_wstate(wstate *w)
{
	free(w);
}
static inline int transfer(cstate *c, void(*cb)(cstate*,int))
{
	int rc;

	if (R_FREE(c) <= 0)
	{
		if (c->rlen > 0)
		{
			printf("%s(peer %s) rcvbuf full\n", c->local, c->server);
			goto wr;
		}
		else
		{
			c->ridx = 0;
		}
	}
	rc = myread(c->w.fd, R_NEXT(c), R_FREE(c));
	if (rc <= 0)
		return -1;
	printf("%s(peer %s) read %d bytes\n", c->local, c->server, rc);
	c->rlen += rc;
wr:
	rc = mywrite_nb(c->pw->fd, c->rbuf + c->ridx, c->rlen);
	if (rc < 0)
	{
		return -1;
	}
	printf("%s(peer %s) write %d bytes\n", c->local, c->server, rc);
	if (rc == c->rlen)
	{
		c->ridx = c->rlen = 0;
	}
	else
	{
		c->rlen -= rc;
		c->ridx += rc;
		//need to setup write watcher to avoid transfer hang
		if (cb)
		{
			printf("setup write cb for %s(peer %s)\n", c->server, c->local);
			cb(c, c->pw->fd);
		}
	}
	return 0;
}

#endif
