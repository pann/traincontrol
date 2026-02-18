#pragma once

#include <stdint.h>
#include <stddef.h>

/* Address families */
#define AF_INET     2

/* Socket types */
#define SOCK_STREAM 1

/* Protocol */
#define IPPROTO_TCP 6

/* Socket options */
#define SOL_SOCKET      0xFFF
#define SO_REUSEADDR    0x0004
#define SO_RCVTIMEO     0x1006

/* Shutdown modes */
#define SHUT_RDWR       2

/* Byte order macros (host is little-endian on x86 and xtensa) */
#define htons(x) ((uint16_t)(((x) >> 8) | (((x) & 0xFF) << 8)))
#define htonl(x) ((uint32_t)( \
    (((x) >> 24) & 0xFF) | \
    (((x) >>  8) & 0xFF00) | \
    (((x) & 0xFF00) <<  8) | \
    (((x) & 0xFF)   << 24)))
#define ntohs(x) htons(x)
#define ntohl(x) htonl(x)

#define INADDR_ANY 0

typedef uint32_t socklen_t;
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
typedef uint16_t sa_family_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t   sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

/* Socket function declarations (faked in fakes_state.c) */
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
int setsockopt(int sockfd, int level, int optname, const void *optval,
               socklen_t optlen);
int close(int fd);
int shutdown(int sockfd, int how);

/* inet_ntoa helper */
char *inet_ntoa(struct in_addr in);
