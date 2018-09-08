#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include "mysocket.h"

#define RELAY_PORT	12345

#define ok_or_goto(v, l) if (!(v)) goto l;

static inline void parse_ip_port(char *str, uint32_t *ip, uint16_t *port)
{
	int n;
	char *l, *r;
	char *ips, *ports;

	l = strchr(str, ':');
	if (!l)
	{
		exit(1);
	}
	r = strrchr(str, ':');
	/* r cannot be NULL */
	if (l != r)
	{
		exit(1);
	}

	if (!strcmp(str, ":"))
	{
		/* all defaults */
		return;
	}
	*l = 0;
	ips = str;
	ports = l + 1;
	if (*ports)
	{
		int val;
		val = atoi(ports);
		if (val < 0 || val > 0x0ffff)
		{
			exit(1);
		}
		*port = val;
	}
	if (*ips)
	{
		if (my_pton4(ips, ip) != 0)
		{
			exit(1);
		}
	}
}

static inline void setup_signals(void)
{
	int rc;
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	rc = sigaction(SIGPIPE, &act, NULL);
	if (rc)
	{
		printf("sigaction: %m\n");
		exit(1);
	}
}

static inline void hexdump(uint8_t *buf, int len)
{
	int i;
	printf("\n");
	for (i = 0; i < len; i++)
	{
		printf("%02x ", buf[i]);
		if ((i+1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
}
#endif
