#ifndef  TC_UTIL_INCLUDED
#define  TC_UTIL_INCLUDED

#include <xcopy.h>


#define TCP_HDR_LEN(tcph) (tcph->doff << 2)
#define IP_HDR_LEN(iph) (iph->ihl << 2) 
#define EXTRACT_32BITS(p)   ((uint32_t)ntohl(*(uint32_t *)(p)))

#define TCP_PAYLOAD_LENGTH(iph, tcph) \
        (ntohs(iph->tot_len) - IP_HDR_LEN(iph) - TCP_HDR_LEN(tcph))

unsigned short csum (unsigned short *packet, int pack_len);
unsigned short tcpcsum(unsigned char *iphdr, unsigned short *packet,
        int pack_len);

int get_l2_len(const unsigned char *frame, const int datalink);
unsigned char *
get_ip_data(pcap_t *pcap, unsigned char *frame, const int pkt_len, 
        int *p_l2_len);

#endif   /* ----- #ifndef TC_UTIL_INCLUDED  ----- */

