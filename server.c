#include <ev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <linux/limits.h>
#include "mylog.h"
#include "mysocket.h"
#include "socks.h"
#include "cstate.h"
#include "common.h"

static struct ev_loop *loop;
static ev_io client_watcher;

uint16_t local_port = RELAY_PORT;
uint32_t local_server = INADDR_ANY;
int backlog = 1024;
int foreground = 0;
FILE *logfile;
int debuglvl = 0;

static char *http_fake_response =
"HTTP/1.1 200 OK\r\n"
"Date: Mon, 23 May 2005 22:38:34 GMT\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"Content-Encoding: UTF-8\r\n"
"Content-Length: 118\r\n"
"Last-Modified: Wed, 08 Jan 2003 23:11:55 GMT\r\n"
"Server: Apache/1.3.3.7 (Unix) (Red-Hat/Linux)\r\n"
"ETag: \"3f80f-1b6-3e1cb03b\"\r\n"
"Accept-Ranges: bytes\r\n"
"Connection: close\r\n\r\n"
"<html><head><title>An Example Page</title></head><body>Hello World, this is a very simple HTML document.</body></html>";
static char http_fake_buf[512];
static int http_fake_len;

static void usage()
{
	mylog("Usage:\n"
		"-h			help\n"
		"-d			log level(1 info, 2 trace)\n"
		"-l			ip:port\n");
	exit(1);
}

static void parse_args(int argc, char **argv)
{
	int opt;
	
	while ((opt = getopt(argc, argv, "hd:Fl:t:")) != -1)
	{
		switch (opt)
		{
			case 'h':
				usage();
				break;
			case 'd':
				debuglvl = atoi(optarg);
				assert(debuglvl >= 0);
			case 'F':
				foreground = 1;
				break;
			case 'l':
				parse_ip_port(optarg, &local_server, &local_port);
				break;
			default:
				usage();
				break;
		}
	}
}
/* @return
 * >= 0 done; -EINPROGRESS request inflight; < 0 error;
 */
static int relay_ipv4(uint8_t *buf, int *sock)
{
	return open_ipv4_nb(ntohl(*(uint32_t*)buf), ntohs(*(uint16_t*)(buf+4)), sock);
}
/* @return
 * >= 0 done; -EINPROGRESS request inflight; < 0 error;
 */
static int relay_domain(uint8_t *buf, int *sock)
{
	return open_domain_nb(buf, ntohs(*(uint16_t*)(buf+strlen(buf)+1)), sock);
}
static void write_cb(EV_P_ ev_io *w, int events)
{
	wstate *ws = (wstate*)w;
	cstate *c = ws->c;
	int rc;

	mytrace("write_cb called for %s(peer %s)\n", c->server, c->local);

	rc = mywrite_nb(w->fd, R_IDX(c), c->rlen);
	if (rc < 0)
	{
		goto fin;
	}
	if (rc == c->rlen)
	{
		c->rlen = c->ridx = 0;
		ev_io_stop(EV_A_ w);
		del_wstate(ws);
		//enable peer read
		ev_io_start(loop, &c->w);
	}
	else
	{
		c->rlen -= rc;
		c->ridx += rc;
	}
	return;
fin:
	ev_io_stop(EV_A_ w);
	del_wstate(ws);
	close(c->w.fd);
	ev_io_stop(EV_A_ &c->w);
	if (c->pw)
	{
		close(c->pw->fd);
		ev_io_stop(EV_A_ c->pw);
		del_cstate((cstate*)c->pw);
	}
	del_cstate(c);
}
//setup write watcher to avoid transfer hang
static void setup_write_cb(cstate *c, int fd)
{
	wstate *ws;

	ws = new_wstate();
	ws->c = c;
	ev_io_stop(loop, &c->w);
	ev_io_init(&ws->w, write_cb, fd, EV_WRITE);
	//disable peer read
	ev_io_start(loop, &ws->w);
}
static void relay_cb(EV_P_ ev_io *w, int events)
{
	cstate *c = (cstate*)w;
	cstate *pc = (cstate*)c->pw;
	int rc;

	if (c->state == CS_DATA)
	{
		mytrace("%s(peer %s) readable\n", c->local, c->server);
		rc = transfer(c, setup_write_cb);
		if (rc < 0)
		{
			goto fin;
		}
	}
	else if (c->state == CS_SOCKS_REQUEST_INPROGESS)
	{
		int err;
		int len = sizeof(int);
		rc = getsockopt(w->fd, SOL_SOCKET, SO_ERROR, &err, &len);
		if (rc != 0)
		{
			myerr("RELAY: getsockopt: %m\n");
			goto fin;
		}
		if (err != 0)
		{
			myerr("RELAY: connect: %s\n", strerror(err));
			goto fin;
		}

		rc = mywrite_nb(pc->w.fd, pc->wbuf, pc->wlen);
		if (rc < 0 || rc < pc->wlen)
		{
			myerr("RELAY: write\n");
			goto fin;
		}
		else
		{
			pc->wlen = pc->widx = 0;
		}

		rc = w->fd;
		ev_io_stop(EV_A_ w);
		ev_io_init(w, relay_cb, rc, EV_READ);
		ev_io_start(loop, w);
		mytrace("RELAY: connected\n");
		c->state = CS_DATA;
		pc->state = CS_DATA;
		mytrace("into CS_DATA\n");
	}
	else
	{
		myerr("RELAY: bad state: %d\n", c->state);
		goto fin;
	}
	return;
fin:
	close(w->fd);
	ev_io_stop(EV_A_ w);
	if (c->pw)
	{
		close(c->pw->fd);
		ev_io_stop(EV_A_ c->pw);
		del_cstate((cstate*)c->pw);
	}
	del_cstate(c);
}
static void client_cb(EV_P_ ev_io *w, int events)
{
	cstate *c = (cstate*)w;
	int rc;

	switch (c->state)
	{
		case CS_DATA:
		{
			mytrace("%s(peer %s) readable\n", c->local, c->server);
			rc = transfer(c, setup_write_cb);
			if (rc < 0)
				goto fin;
		}
			break;
		case CS_SOCKS_REQUEST:
		{
			rc = myread(w->fd, R_NEXT(c), R_FREE(c));
			ok_or_goto((rc > 0), fin);
			c->rlen += rc;
			rc = socks_do_request_server(c->rbuf + c->ridx, c->rlen, c->wbuf, &c->wlen, c->server);
			if (rc >= 0)	//domain or ipv4
			{
				//rbuf == request
				int fd;
				if (rc > 0)
				{
					rc = relay_domain(c->rbuf + c->ridx, &fd);
				}
				else
				{
					rc = relay_ipv4(c->rbuf + c->ridx, &fd);
				}
				if (rc < 0 && rc != -EINPROGRESS)
				{
					myerr("REQUEST: relay\n");
					goto fin;
				}
				cstate *pc = new_cstate();
				pc->pw = w;
				strcpy(pc->server, c->local);
				strcpy(pc->local, c->server);
				c->pw = &pc->w;
				if (rc >= 0)
				{
					pc->state = CS_DATA;
					c->state = CS_DATA;
					c->rlen = c->ridx = 0;
					ev_io_init(&pc->w, relay_cb, fd, EV_READ);
					ev_io_start(loop, &pc->w);
					mylog("into CS_DATA\n");
					//wbuf == reply
					rc = mywrite(w->fd, c->wbuf, c->wlen);
					if (rc < 0 || rc < c->wlen)
					{
						myerr("REQUEST: write\n");
						goto fin;
					}
					c->wlen = c->widx = 0;
				}
				else
				{
					pc->state = c->state = CS_SOCKS_REQUEST_INPROGESS;
					ev_io_init(&pc->w, relay_cb, fd, EV_WRITE);
					ev_io_start(loop, &pc->w);
					mytrace("REQUEST: in progress\n");
				}
				c->rlen = c->ridx = 0;
			}
			else if (rc == -1)	//error
			{
				myerr("%s sent invalid request\n", c->local);
				hexdump(R_IDX(c), c->rlen);
				myerr("%*s\n", c->rlen, R_IDX(c));
				mywrite(w->fd, http_fake_buf, http_fake_len);
				goto fin;
			}
			//-100
			//incomplete message
		}
			break;
		case CS_SOCKS_REQUEST_INPROGESS:
		{
			//may reach here when connection reset
			myerr("client comes while request in progress\n");
			goto fin;
		}
			break;
		default:
		{
			myerr("invalid state: %d\n", c->state);
			goto fin;
		}
			break;
	}
	return;
fin:
	close(w->fd);
	ev_io_stop(EV_A_ w);
	if (c->pw)
	{
		close(c->pw->fd);
		ev_io_stop(EV_A_ c->pw);
		del_cstate((cstate*)c->pw);
	}
	del_cstate(c);
}
static void listen_cb(EV_P_ ev_io *w, int events)
{
	int fd;
	struct sockaddr rmt;
	socklen_t len;
	cstate *c;

	len = sizeof(rmt);
	fd = myaccept(w->fd, &rmt, &len);
	if (fd < 0)
	{
		ev_break(EV_A_ EVBREAK_ALL);
		return;
	}
	fd_set_nonblock(fd);
	c = new_cstate();
	sa2str(&rmt, c->local);
	c->state = CS_SOCKS_REQUEST;
	sa2str(&rmt, c->local);
	ev_io_init(&c->w, &client_cb, fd, EV_READ);
	ev_io_start(loop, &c->w);
}
static void setup_listener(void)
{
	int fd;

	fd = mylisten(local_server, local_port, backlog);
	if (fd < 0)
	{
		exit(1);
	}
	ev_io_init(&client_watcher, &listen_cb, fd, EV_READ);
	ev_io_start(loop, &client_watcher);
}
int main(int argc, char **argv)
{
	logfile = stdout;

	loop = EV_DEFAULT;

	http_fake_len = sprintf(http_fake_buf, "%s", http_fake_response);
	assert(http_fake_len > 0);

	parse_args(argc, argv);
	
	setup_signals();
	setup_listener();

	ev_run(loop, 0);

	return 0;
}