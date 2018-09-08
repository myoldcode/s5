#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "mysocket.h"

int myconnect(struct sockaddr *addr, socklen_t len, uint16_t port)
{
	int fd;
	int rc;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		printf("socket6: %s\n", strerror(errno));
		return -1;
	}
	((struct sockaddr_in*)addr)->sin_port = htons(port);
	rc = connect(fd, addr, len);
	if (rc < 0)
	{
		printf("connect4: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}
int myconnect6(struct sockaddr *addr, socklen_t len, uint16_t port)
{
	int fd;
	int rc;

	fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0)
	{
		printf("socket6: %s\n", strerror(errno));
		return -1;
	}
	((struct sockaddr_in6*)addr)->sin6_port = htons(port);
	rc = connect(fd, addr, len);
	if (rc < 0)
	{
		printf("connect6: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

#if 0
static int get_domain_addr(uint8_t *domain, struct addrinfo **res)
{
	int rc;

	rc = getaddrinfo(domain, NULL, NULL, res);
	if (rc == 0)
	{
		return 0;
	}
	else
	{
		printf("getaddrinfo: %s\n", gai_strerror(rc));
	}
	return -1;
}

int myconnect_domain(uint8_t *domain, uint16_t port)
{
	struct addrinfo *res, *rp;
	struct sockaddr addr;
	int rc;

	rc = get_domain_addr(domain, &res);
	if (rc < 0)
	{
		return -1;
	}
	if (res->ai_family == AF_INET)
		rc = myconnect(res->ai_addr, res->ai_addrlen, port);
	else
		rc = myconnect6(res->ai_addr, res->ai_addrlen, port);
	freeaddrinfo(res);
	return rc;
}
#else
int get_domain_addr(uint8_t *domain, struct hostent **ent)
{
	*ent = gethostbyname(domain);
	if (!*ent)
	{
		printf("gethostbyname: %m :%s\n", hstrerror(h_errno));
		return -1;
	}
	return 0;
}
int myconnect_domain(uint8_t *domain, uint16_t port)
{
	struct hostent *ent;
	int rc;

	rc = get_domain_addr(domain, &ent);
	if (rc < 0)
	{
		return -1;
	}
	if (ent->h_addrtype == AF_INET)
	{
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		in.sin_addr.s_addr = *(uint32_t*)ent->h_addr_list[0];
		rc = myconnect((struct sockaddr*)&in, sizeof(in), port);
	}
	else
	{
		struct sockaddr_in6 in6;
		memset(&in6, 0, sizeof(in6));
		in6.sin6_family = AF_INET6;
		memcpy(&in6.sin6_addr.__in6_u.__u6_addr8, ent->h_addr_list[0], ent->h_length);
		rc = myconnect6((struct sockaddr*)&in6, sizeof(in6), port);
	}
	return rc;
}
#endif
int open_ipv4(uint32_t addr, uint16_t port)
{
	struct sockaddr_in in; 
	memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	in.sin_port = htons(port);
	in.sin_addr.s_addr = htonl(addr);
	return myconnect((struct sockaddr*)&in, sizeof(in), port);
}
/* @return
 * >= 0 done; -EINPROGRESS connect inflight; < 0 error;
 */
int open_ipv4_nb(uint32_t addr, uint16_t port, int *sock)
{
	int fd;
	int rc;
	struct sockaddr_in in;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		printf("socket: %m\n");
		return -1;
	}
	fd_set_nonblock(fd);
	*sock = fd;

	in.sin_family = AF_INET;
	in.sin_addr.s_addr = htonl(addr);
	in.sin_port = htons(port);
	rc = connect(fd, (struct sockaddr*)&in, sizeof(in));
	if (rc < 0 && errno == EINPROGRESS)
	{
		return -EINPROGRESS;
	}
	if (rc < 0)
	{
		close(fd);
		return -1;
	}
	return 0;
}
int open_domain(uint8_t *domain, uint16_t port)
{
	return myconnect_domain(domain, port);
}
/* @return
 * >= 0 done; -EINPROGRESS connect inflight; < 0 error;
 */
int open_domain_nb(uint8_t *domain, uint16_t port, int *sock)
{
	int fd;
	int rc;
	struct hostent *ent;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		printf("socket: %m\n");
		return -1;
	}
	fd_set_nonblock(fd);
	*sock = fd;

	rc = get_domain_addr(domain, &ent);
	if (rc < 0)
	{
		return -1;
	}
	if (ent->h_addrtype == AF_INET)
	{
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		in.sin_addr.s_addr = *(uint32_t*)ent->h_addr_list[0];
		in.sin_port = htons(port);
		rc = connect(fd, (struct sockaddr*)&in, sizeof(in));
	}
	else
	{
		struct sockaddr_in6 in6;
		memset(&in6, 0, sizeof(in6));
		in6.sin6_family = AF_INET6;
		memcpy(&in6.sin6_addr.__in6_u.__u6_addr8, ent->h_addr_list[0], ent->h_length);
		in6.sin6_port = htons(port);
		rc = connect(fd, (struct sockaddr*)&in6, sizeof(in6));
	}

	if ((rc < 0) && (errno == EINPROGRESS))
	{
		return -EINPROGRESS;
	}
	if (rc < 0)
	{
		close(fd);
		return -1;
	}
	return 0;
}
int mylisten(uint32_t addr, uint16_t port, int backlog)
{
	int fd;
	int rc;
	int one = 1;
	struct sockaddr_in in;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		printf("socket: %s\n", strerror(errno));
		return -1;
	}

	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
	if (rc != 0)
	{
		printf("REUSEADDR: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	in.sin_family = AF_INET;
	in.sin_addr.s_addr = htonl(addr);
	in.sin_port = htons(port);
	rc = bind(fd, (struct sockaddr*)&in, sizeof(in));
	if (rc < 0)
	{
		printf("bind: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	rc = listen(fd, backlog);
	if (rc < 0)
	{
		printf("listen: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

int myaccept(int fd, struct sockaddr *rmt, socklen_t *len)
{
	int rc;

redo:
	rc = accept(fd, rmt, len);
	if (rc < 0)
	{
		if (errno == EINTR)
		{
			goto redo;
		}
		else
		{
			printf("accept: %s\n", strerror(errno));
		}
	}
	return rc;
}

int myread(int fd, uint8_t *buf, size_t len)
{
	int rc;

redo:
	rc = read(fd, buf, len);
	if (rc < 0)
	{
		if (errno == EINTR)
		{
			goto redo;
		}
		else
		{
			printf("read: %s\n", strerror(errno));
		}
	}
	return rc;
}

int mywrite(int fd, uint8_t *buf, size_t len)
{
	int rc;
	int wrote;
	int remain = len;

	while (remain)
	{
		rc = write(fd, buf, remain);
		if (rc < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else if (errno == EPIPE)
			{
				printf("\nEPIPE\n\n");
				return -1;
			}
			else
			{
				printf("write: %s\n", strerror(errno));
				return -1;
			}
		}
		wrote += rc;
		remain -= rc;
	}
	return len;
}
int mywrite_nb(int fd, uint8_t *buf, size_t len)
{
	int rc;
	int wrote;
	int remain = len;

	rc = write(fd, buf, remain);
	if (rc < 0)
	{

		if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
		{
			return 0;
		}
		else
		{
			printf("write: %m\n");
			return -1;
		}
	}
	return rc;
}
int fd_set_nonblock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
	{
		printf("fnctl: %m\n");
		return -1;
	}
	return 0;
}
int tcp_set_keepalive(int sock, int value)
{
	int rc;
	
	rc = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(int));
	if (rc)
	{
		printf("setsockopt: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int my_pton4(char *p, uint32_t *ip)
{
	struct in_addr in;
	int rc;

	rc = inet_pton(AF_INET, p, &in);
	if (rc == 1)
	{
		*ip = ntohl(in.s_addr);
		return 0;
	}
	return -1;
}
/* length(p) should be >= INET6_ADDRSTRLEN */
void ip2str(int type, char *p, uint8_t *ip)
{
	const char *rc;
	rc = inet_ntop(type, ip, p, INET6_ADDRSTRLEN);
	if (rc == NULL)
	{
		printf("ip2str: %m\n");
		snprintf(p, INET6_ADDRSTRLEN, "Unknown");
	}
}
/* length(p) should be >= INET6_ADDRSTRLEN */
void sa2str(struct sockaddr *sa, char *p)
{
	const char *rc;
	if (sa->sa_family == AF_INET)
	{
		rc = inet_ntop(AF_INET, &((struct sockaddr_in*)sa)->sin_addr, p, INET6_ADDRSTRLEN);
	}
	else if (sa->sa_family == AF_INET6)
	{
		rc = inet_ntop(AF_INET6, &((struct sockaddr_in6*)sa)->sin6_addr, p, INET6_ADDRSTRLEN);
	}
	if (rc == NULL)
	{
		printf("sa2str: inet_ntop failed: %m\n");
		snprintf(p, INET6_ADDRSTRLEN, "Unknown");
	}
}
