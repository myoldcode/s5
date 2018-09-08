#include <ev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <linux/limits.h>
#include "mysocket.h"
#include "socks.h"
#include "cstate.h"
#include "common.h"

static struct ev_loop *loop;
static ev_io client_watcher;

uint16_t relay_port = RELAY_PORT;
uint32_t relay_server = INADDR_ANY;
uint16_t local_port = RELAY_PORT;
uint32_t local_server = INADDR_ANY;
int backlog = 1024;
int foreground = 0;
FILE *logfile;
int debuglvl = 0;

static void usage()
{
	printf("Usage:\n"
		"-h			help\n"
		"-F			foreground\n"
		"-d			log level(1 info, 2 trace)\n"
		"-l			ip:port\n"
		"-t			ip:port\n");
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
				break;
			case 'F':
				foreground = 1;
				break;
			case 'l':
				parse_ip_port(optarg, &local_server, &local_port);
				break;
			case 't':
				parse_ip_port(optarg, &relay_server, &relay_port);
				break;
			default:
				usage();
				break;
		}
	}
}

static int relay_request(uint8_t *buf, int len, int *sock)
{
	return open_ipv4_nb(relay_server, relay_port, sock);
}
static void write_cb(EV_P_ ev_io *w, int events)
{
	wstate *ws = (wstate*)w;
	cstate *c = ws->c;
	int rc;

	myerr("write_cb called for %s(peer %s)\n", c->server, c->local);

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
	ev_io_init(&ws->w, write_cb, fd, EV_WRITE);
	ev_io_start(loop, &ws->w);
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

		rc = mywrite_nb(w->fd, pc->rbuf, pc->rlen);
		if (rc < 0 || rc < len)
		{
			myerr("RELAY: write\n");
			goto fin;
		}
		else
		{
			pc->rlen = pc->ridx = 0;
		}

		rc = w->fd;
		ev_io_stop(EV_A_ w);
		ev_io_init(w, relay_cb, rc, EV_READ);
		ev_io_start(loop, w);
		mytrace("RELAY: connected\n");
		c->state = CS_DATA;
		((cstate*)c->pw)->state = CS_DATA;
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
		case CS_SOCKS_METHOD:
		{
			rc = myread(w->fd, R_NEXT(c), R_FREE(c));
			ok_or_goto((rc > 0), fin);
			c->rlen += rc;
			rc = socks_do_method(c->rbuf + c->ridx, c->rlen, c->wbuf, &c->wlen);
			if (rc > 0)
			{
				//method done
				c->rlen = c->ridx = 0;
				rc = mywrite(w->fd, c->wbuf, c->wlen);
				if (rc < 0 || rc < c->wlen)
				{
					//should not block
					myerr("METHOD: write\n");
					goto fin;
				}
				c->wlen = c->widx = 0;
				mytrace("into CS_SOCKS_REQUEST\n");
				c->state = CS_SOCKS_REQUEST;
			}
			else if (rc < 0)
			{
				goto fin;
			}
			//incomplete message
		}
			break;
		case CS_SOCKS_REQUEST:
		{
			rc = myread(w->fd, R_NEXT(c), R_FREE(c));
			ok_or_goto((rc > 0), fin);
			c->rlen += rc;
			rc = socks_do_request(c->rbuf + c->ridx, c->rlen, c->wbuf, &c->wlen, c->server);
			if (rc > 0)
			{
				//rc == request len; c->rlen may be bigger than request?
				//rbuf == request
				int fd;
				rc = relay_request(R_IDX(c), rc, &fd);
				if (rc < 0 && rc != -EINPROGRESS)
				{
					myerr("REQUEST: relay\n");
					goto fin;
				}
				cstate *pc = new_cstate();
				strcpy(pc->server, c->local);
				strcpy(pc->local, c->server);
				pc->pw = w;
				c->pw = &pc->w;
				if (rc >= 0)
				{
					pc->state = CS_DATA;
					c->state = CS_DATA;
					c->rlen = c->ridx = 0;
					ev_io_init(&pc->w, relay_cb, fd, EV_READ);
					ev_io_start(loop, &pc->w);
					mytrace("into CS_DATA\n");
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
			}
			else if (rc < 0)
			{
				goto fin;
			}
			//incomplete message
		}
			break;
		case CS_SOCKS_REQUEST_INPROGESS:
		{
			//may reach here when relay timeout
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
	c->state = CS_SOCKS_METHOD;
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

	parse_args(argc, argv);
	if (!foreground)
	{
		if (daemon(0, 0))
		{
			myerr("daemon: %m\n");
			exit(1);
		}
	}
	
	setup_signals();
	setup_listener();

	ev_run(loop, 0);

	return 0;
}
