#ifndef QSHIM_SYS_SOCKET_H
#define QSHIM_SYS_SOCKET_H
/* BSD-derived io-pkt ABI.  ref: try_connect_once (connector_main @0x35c4..)
 * socket(2, 1, 0); setsockopt(fd, 0xffff, 0x1005, &timeval, 8);
 * shutdown(fd, 2) in disconnect_for_shutdown. */
#include <stddef.h>
#include <unistd.h>
typedef unsigned int socklen_t;
typedef unsigned char sa_family_t;
struct sockaddr { unsigned char sa_len; sa_family_t sa_family; char sa_data[14]; };
#define AF_INET      2
#define SOCK_STREAM  1
#define SOL_SOCKET   0xffff
#define SO_SNDTIMEO  0x1005
#define SHUT_RDWR    2
extern int socket(int domain, int type, int protocol);
extern int connect(int fd, const struct sockaddr *addr, socklen_t len);
extern int setsockopt(int fd, int level, int name, const void *val, socklen_t len);
extern int shutdown(int fd, int how);
extern ssize_t send(int fd, const void *buf, size_t len, int flags);
extern ssize_t recv(int fd, void *buf, size_t len, int flags);
/* MSG_NOSIGNAL is deliberately absent: io-pkt rejects it (see bus.c). */
#endif
