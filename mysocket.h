#ifndef _MYSOCKET_H_
#define _MYSOCKET_H_

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int myconnect(struct sockaddr*, socklen_t, uint16_t);
int myconnect6(struct sockaddr*, socklen_t, uint16_t);
int myconnect_domain(uint8_t *, uint16_t);
int open_ipv4(uint32_t, uint16_t);
int open_ipv4_nb(uint32_t addr, uint16_t port, int *sock);
int open_domain(uint8_t *, uint16_t);
int open_domain_nb(uint8_t *domain, uint16_t port, int *sock);
int get_domain_addr(uint8_t *domain, struct hostent **ent);
int mylisten(uint32_t, uint16_t, int);
int myaccept(int, struct sockaddr *, socklen_t *);
int myread(int, uint8_t *, size_t);
int mywrite(int, uint8_t *, size_t);
int mywrite_nb(int, uint8_t *, size_t);

int tcp_set_keepalive(int, int);
int fd_set_nonblock(int);
int my_pton4(char *, uint32_t *);
void ip2str(int type, char *p, uint8_t *ip);
void sa2str(struct sockaddr *sa, char *p);

#endif
