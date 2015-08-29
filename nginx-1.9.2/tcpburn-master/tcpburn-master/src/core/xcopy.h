#ifndef  XCOPY_H_INCLUDED
#define  XCOPY_H_INCLUDED

#include <tc_auto_config.h>
#include <limits.h>
#include <asm/types.h>
#include <stdarg.h>
#include <stddef.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <pcap.h>

#define VERSION "1.0.0"

#define INTERNAL_VERSION 6

typedef struct tc_pool_s        tc_pool_t;
typedef struct tc_buf_s         tc_buf_t;

#define TC_MAX_ALLOC_FROM_POOL  (tc_pagesize - 1)
#define TC_DEFAULT_POOL_SIZE    (16 * 1024)
#define TC_POOL_ALIGNMENT       16
#define TC_MIN_POOL_SIZE                                                     \
            tc_align((sizeof(tc_pool_t) + 2 * sizeof(tc_pool_large_t)),      \
                                                  TC_POOL_ALIGNMENT)

#define ETHER_ADDR_LEN 0x6

#define DEFAULT_MTU   1500
#define DEFAULT_MSS   1460
#define MAX_FRAME_LENGTH 65550
#define SERVER_PORT   36524

#define DEFAULT_CONN_INIT_SP_FACT 1024
#define FIRST_PORT 32768
#define LAST_PORT 61000
#define VALID_PORTS_NUM (LAST_PORT - FIRST_PORT + 1)

#define MAX_REAL_SERVERS 256
#define MAX_PCAP_FILES 1024

#define DEFAULT_SESSION_TIMEOUT 75
#define DEFAULT_TIMEOUT 120
#define FIN_TIMEOUT 6000

#define CHECK_INTERVAL  50
#define OUTPUT_INTERVAL  5000

#define MAX_WRITE_TRIES 1024
#define MAX_READ_LOG_TRIES 65536

#define SLOT_MAX 256
#define SLOT_AVG 32 

#define SLP_THRSH_NUM 4
#define M_CLIENT_IP_NUM 65536

/* max fd number for select */
#define MAX_FD_NUM    1024
#define MAX_FD_VALUE  (MAX_FD_NUM-1)
#define MAX_CONNECTION_NUM 16

#define COMB_MAX_NUM 16
#define COMB_LENGTH (COMB_MAX_NUM * MSG_SERVER_SIZE)

#define SRC_DIRECTION 0
#define DST_DIRECTION 1

#define MAX_ALLOWED_IP_NUM 32

#define  CLIENT_ADD   1
#define  CLIENT_DEL   2

#define UNKNOWN 0
#define REMOTE  1
#define LOCAL   2

#define CHECK_DEST 1
#define CHECK_SRC  2

#define TYPE_DEFAULT 0
#define TYPE_DELAY_ACK 1
#define TYPE_RTO 2
#define TYPE_ACT 3
#define TYPE_DELAY_IGNITE 4
#define TYPE_DELAY_OVER 5

#define DEF_TOPO_WAIT 10 

#if (TC_COMET)
#define DEFAULT_RTO 1000
#else
#define DEFAULT_RTO 100
#endif

#define BREAK_NUM 20000
#define HALF_MILLION 500000
#define MILLION 1000000

#define TCP_HEADER_DOFF_MIN_VALUE 5
#define TCP_HEADER_DOFF_MSS_VALUE 6
#define TCP_HEADER_DOFF_TS_VALUE 8
#define TCP_HEADER_DOFF_WS_TS_VALUE 9


typedef volatile sig_atomic_t tc_atomic_t;

typedef struct iphdr  tc_iph_t;
typedef struct tcphdr tc_tcph_t;

/* 
 * the 40 bytes available for TCP options 
 * we support 24 bytes for TCP options
 */
#define MAX_OPTION_LEN 24
#define TCPOPT_WSCALE 3

#define RESP_HEADER_SIZE (sizeof(tc_iph_t) + sizeof(tc_tcph_t) + MAX_OPTION_LEN)
#define RESP_MAX_USEFUL_SIZE RESP_HEADER_SIZE

/* bool constants */
#if (HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#define bool char
#define false 0
#define true 1
#endif /* HAVE_STDBOOL_H */ 

enum sess_status{
    CLOSED            = 0,
    SYN_SENT          = 1,
    SYN_CONFIRM       = 2,
    SYN_ACK           = 4,
    SEND_REQ          = 8,
    RECV_RESP         = 16,
    SERVER_FIN        = 32,
    CLIENT_FIN        = 64
};

enum packet_classification{
    CLIENT_FLAG,
    RESERVED_CLIENT_FLAG,
    BACKEND_FLAG,
    FAKED_CLIENT_FLAG,
    TO_BAKEND_FLAG,
    UNKNOWN_FLAG
};

#define SESS_OVER_STATE (SERVER_FIN | CLIENT_FIN)

#define ETHER_ADDR_STR_LEN 17

#ifndef ETHERTYPE_VLAN
#define ETHERTYPE_VLAN 0x8100  /* IEEE 802.1Q VLAN tagging */
#endif

#define CISCO_HDLC_LEN 4
#define SLL_HDR_LEN 16
#define IP_RECV_BUF_SIZE 65536
#define ETHERNET_HDR_LEN (sizeof(struct ethernet_hdr))
#define DEFAULT_DEVICE     "any"

#define IP_HEADER_LEN sizeof(tc_iph_t)
#define TCP_HEADER_MIN_LEN sizeof(tc_tcph_t)
#define FAKE_FRAME_LEN (60 + ETHERNET_HDR_LEN)
#define FAKE_MIN_IP_DATAGRAM_LEN (IP_HEADER_LEN + (TCP_HEADER_DOFF_MIN_VALUE << 2))
#define FAKE_IP_TS_DATAGRAM_LEN (IP_HEADER_LEN + (TCP_HEADER_DOFF_TS_VALUE << 2))
#define FAKE_SYN_IP_DATAGRAM_LEN (IP_HEADER_LEN + (TCP_HEADER_DOFF_MSS_VALUE << 2))
#define FAKE_SYN_IP_TS_DATAGRAM_LEN (IP_HEADER_LEN + (TCP_HEADER_DOFF_WS_TS_VALUE << 2))

/*  
 *  Ethernet II header
 *  static header size: 14 bytes          
 */ 
struct ethernet_hdr {
    uint8_t  ether_dhost[ETHER_ADDR_LEN];
    uint8_t  ether_shost[ETHER_ADDR_LEN];
    uint16_t ether_type;                 
};

/* receiving buffer size for response */
#define CAPTURE_RESP_HEADER_MAX_LEN 120
#define CAPTURE_RESP_MAX_SIZE CAPTURE_RESP_HEADER_MAX_LEN
#define RESP_RECV_BUF_SIZE (CAPTURE_RESP_MAX_SIZE)


typedef struct connections_s{
    int index; 
    int num;
    int remained_num;
    int fds[MAX_CONNECTION_NUM];
}connections_t;

#define TIMER_INTERVAL 1

/* global functions */
int daemonize();
int before(uint32_t seq1, uint32_t seq2);

#define after(seq2, seq1) before(seq1, seq2)

#define TC_OK        0
#define TC_ERROR    -1
#define TC_ERR_EXIT  1
#define TC_DECLINED -2

#define tc_cpymem(d, s, l) (((char *) memcpy(d, (void *) s, l)) + (l))
#define tc_memzero(d, l) (memset(d, 0, l))
#define tc_abs(value)       (((value) >= 0) ? (value) : - (value))
#define tc_max(val1, val2)  ((val1 < val2) ? (val2) : (val1))
#define tc_min(val1, val2)  ((val1 > val2) ? (val2) : (val1))

#include <tc_config.h>
#include <tc_log.h>
#include <tc_rbtree.h>
#include <tc_alloc.h>
#include <tc_link_list.h>
#include <tc_palloc.h>
#include <tc_time.h>
#include <tc_signal.h>
#include <tc_event.h>
#include <tc_event_timer.h>
#include <tc_select_module.h>
#include <tc_msg.h>
#include <tc_socket.h>


#endif /* XCOPY_H_INCLUDED */

