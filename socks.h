#ifndef _SOCKS_H_
#define _SOCKS_H_
#include <stdint.h>
#include <string.h>

int socks_do_method(uint8_t *rbuf, uint16_t rlen, uint8_t *wbuf, uint16_t *wlen);
int socks_do_request(uint8_t *rbuf, uint16_t rlen, uint8_t *wbuf, uint16_t *wlen, uint8_t *server);
int socks_do_request_server(uint8_t *rbuf, uint16_t rlen, uint8_t *wbuf, uint16_t *wlen, uint8_t *server);

#endif
