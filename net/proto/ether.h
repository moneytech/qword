#ifndef __NET_ETHER_H__
#define __NET_ETHER_H__

#include <net/net.h>

struct ether_hdr {
    mac_addr_t dst;
    mac_addr_t src;
    uint16_t type;
#define ETHER_IPV4  HTONS(0x0800)
#define ETHER_ARP   HTONS(0x0806)
} __attribute__((packed));

#endif //__NET_ETHER_H__
