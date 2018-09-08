#include "socks.h"
#include "mysocket.h"

#define SOCKS5_VERSION	5
#define CMD_CONNECT		1
#define CMD_BIND		2
#define CMD_UDP_ASSOC	3

/* @return
 * 0 incomplete message; > 0 done; < 0 error;
 */
int socks_do_method(uint8_t *rbuf, uint16_t rlen, uint8_t *wbuf, uint16_t *wlen)
{
	uint8_t mlen;
	uint8_t *methods;
	int i;

	if (rlen < 2)
		return 0;
	mlen = rbuf[1];
	if (mlen > rlen)
		return 0;
	//message is complete
	methods = &rbuf[2];
	for ( i = 0; i < mlen; i++)
	{
		if (methods[i] == 0)
			break;
	}
	if (i == mlen)
	{
		printf("only support method X'00'\n");
		return -1;
	}
	wbuf[0] = SOCKS5_VERSION;
	wbuf[1] = 0;
	*wlen = 2;
	return 1;
}
/* @return
 * 0 incomplete message; > 0 message length; < 0 error;
 */
int socks_do_request(uint8_t *rbuf, uint16_t rlen, uint8_t *wbuf, uint16_t *wlen, uint8_t *server)
{
	uint8_t mlen;
	uint8_t type;

	if (rlen < 5)
		return 0;
	if (rbuf[1] != CMD_CONNECT)
	{
		printf("only support CONNECT\n");
		return -1;
	}
	type = rbuf[3];
	switch (type)
	{
		case 1:
			if (rlen < 10)
				return 0;
			mlen = 10;
			ip2str(AF_INET, server, &rbuf[4]);
			break;
		case 3:
			mlen = rbuf[4] + 7;
			if (rlen < mlen)
				return 0;
			memcpy(server, &rbuf[5], rbuf[4]);
			server[rbuf[4]] = 0;
			break;
		case 4:
			//fallthru, no ipv6 support
		default:
			printf("bad address type: %d\n", type);
			return -1;
			break;
	}
    wbuf[0] = SOCKS5_VERSION;
    wbuf[1] = 0; wbuf[2] = 0; wbuf[3] = 1;
    wbuf[4] = 0; wbuf[5] = 0; wbuf[6] = 0;
    wbuf[7] = 0; wbuf[8] = 0; wbuf[9] = 0;
    *wlen = 10;

	return mlen;
}
/* @return
 * 0 ipv4; > 0 domain; < 0 error;
 */
int socks_do_request_server(uint8_t *rbuf, uint16_t rlen, uint8_t *wbuf, uint16_t *wlen, uint8_t *server)
{
	uint8_t mlen;
	uint8_t type;

	if (rlen < 5)
		return -100;
	if (rbuf[1] != CMD_CONNECT)
	{
		printf("only support CONNECT\n");
		return -1;
	}
	type = rbuf[3];
	switch (type)
	{
		case 1:
			if (rlen < 10)
				return -100;
			mlen = 0;
			ip2str(AF_INET, server, &rbuf[4]);
			memcpy(rbuf, &rbuf[4], 6);
			break;
		case 3:
			mlen = rbuf[4] + 7;
			if (rlen < mlen)
				return -100;
			mlen = rbuf[4];
			memmove(rbuf, &rbuf[5], mlen+2);
			rbuf[mlen+2] = rbuf[mlen+1];
			rbuf[mlen+1] = rbuf[mlen];
			rbuf[mlen] = 0;
			strcpy(server, rbuf);
			break;
		case 4:
			//fallthru, no ipv6 support
		default:
			printf("bad address type: %d\n", type);
			return -1;
			break;
	}

    wbuf[0] = SOCKS5_VERSION;
    wbuf[1] = 0; wbuf[2] = 0; wbuf[3] = 1;
    wbuf[4] = 0; wbuf[5] = 0; wbuf[6] = 0;
    wbuf[7] = 0; wbuf[8] = 0; wbuf[9] = 0;
    *wlen = 10;

	return mlen;
}
