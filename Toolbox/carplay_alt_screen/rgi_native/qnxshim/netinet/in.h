#ifndef QSHIM_NETINET_IN_H
#define QSHIM_NETINET_IN_H
/* ref: connector_main @0x3620..0x364c: family byte at +1 (sin_len at +0),
 * port halfword at +2, inet_addr() result at +4, connect(..., 16). */
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
struct in_addr { in_addr_t s_addr; };
struct sockaddr_in {
    uint8_t     sin_len;
    sa_family_t sin_family;
    in_port_t   sin_port;
    struct in_addr sin_addr;
    char        sin_zero[8];
};
#define QSHIM_IN_ASSERT(name, c) typedef char qshim_in_assert_##name[(c) ? 1 : -1]
QSHIM_IN_ASSERT(sockaddr_in_16, sizeof(struct sockaddr_in) == 16u);
QSHIM_IN_ASSERT(family_1, offsetof(struct sockaddr_in, sin_family) == 1u);
QSHIM_IN_ASSERT(port_2, offsetof(struct sockaddr_in, sin_port) == 2u);
QSHIM_IN_ASSERT(addr_4, offsetof(struct sockaddr_in, sin_addr) == 4u);
#undef QSHIM_IN_ASSERT
#define IPPROTO_TCP 6   /* ref: setsockopt(fd, 6, 1, &one, 4) @0x35d8 */
/* Little-endian target; QNX implements these as macros, nothing is imported. */
#define htons(x) __builtin_bswap16((uint16_t)(x))
#define ntohs(x) __builtin_bswap16((uint16_t)(x))
#define htonl(x) __builtin_bswap32((uint32_t)(x))
#define ntohl(x) __builtin_bswap32((uint32_t)(x))
#endif
