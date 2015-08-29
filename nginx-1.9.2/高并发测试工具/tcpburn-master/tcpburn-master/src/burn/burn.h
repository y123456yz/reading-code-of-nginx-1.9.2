#ifndef BURN_INCLUDED
#define BURN_INCLUDED 

#define LOCALHOST (inet_addr("127.0.0.1"))

typedef struct {
    char file[256];
} tc_pcap_file;

typedef struct {
    /* online ip from the client perspective */
    uint32_t      online_ip;
    uint32_t      target_ip;
    uint16_t      online_port;
    uint16_t      target_port;
    unsigned char src_mac[ETHER_ADDR_LEN];
    unsigned char dst_mac[ETHER_ADDR_LEN];
} ip_port_pair_mapping_t;


typedef struct {
    int                      num;
    ip_port_pair_mapping_t **mappings;
} ip_port_pair_mappings_t;

typedef struct real_ip_addr_s {
    int           num;
    int           active_num;
    short         active[MAX_REAL_SERVERS];
    uint32_t      ips[MAX_REAL_SERVERS];
    uint16_t      ports[MAX_REAL_SERVERS];
    connections_t connections[MAX_REAL_SERVERS];
} real_ip_addr_t;


static inline ip_port_pair_mapping_t *
get_test_pair(ip_port_pair_mappings_t *transfer, uint32_t ip, uint16_t port)
{
    int                     i;
    ip_port_pair_mapping_t *pair, **mappings;

    pair     = NULL;
    mappings = transfer->mappings;
    for (i = 0; i < transfer->num; i++) {
        pair = mappings[i];
        if (ip == pair->online_ip && port == pair->online_port) {
            return pair;
        } else if (pair->online_ip == 0 && port == pair->online_port) {
            return pair;
        }
    }
    return NULL;
}


static inline int
check_pack_src(ip_port_pair_mappings_t *transfer, uint32_t ip,
        uint16_t port, int src_flag)
{
    int                     i, ret;
    ip_port_pair_mapping_t *pair, **mappings;

    ret = UNKNOWN;
    mappings = transfer->mappings;

    for (i = 0; i < transfer->num; i++) {

        pair = mappings[i];
        if (CHECK_DEST == src_flag) {
            if (ip == pair->online_ip && port == pair->online_port) {
                ret = LOCAL;
                break;
            } else if (0 == pair->online_ip && port == pair->online_port) {
                ret = LOCAL;
                break;
            }
        } else if (CHECK_SRC == src_flag) {
            if (ip == pair->target_ip && port == pair->target_port) {
                ret = REMOTE;
                break;
            }
        }
    }

    return ret;
}


typedef struct xcopy_clt_settings {
    unsigned int  mtu:16;               /* MTU sent to backend */
    unsigned int  mss:16;               /* MSS sent to backend */
    unsigned int  par_connections:8;    /* parallel connections */
    unsigned int  client_mode:3;     

    unsigned int  target_localhost:1;
    unsigned int  ignite_complete:1;
    unsigned int  do_daemonize:1;       /* daemon flag */
    unsigned int  percentage:7;         /* percentage of the full flow that 
                                           will be tranfered to the backend */
    unsigned int  sess_timeout:16;      /* max value for sess timeout.
                                           If reaching this value, the sess
                                           will be removed */
    unsigned int  sess_keepalive_timeout:16;  

    tc_pool_t    *pool;
    char         *raw_transfer;         /* online_ip online_port target_ip
                                           target_port string */

    char         *pid_file;             /* pid file */
    char         *log_path;             /* error log path */
    char         *raw_clt_ips;     
#if (TC_TOPO)
    link_list    *s_list;
    int           topo_time_diff;     
#endif
    int           valid_ip_num;              
    uint32_t      valid_ips[M_CLIENT_IP_NUM];              
    uint32_t      sess_ms_timeout;
    int           users;
    int           throughput_factor;   
    int           accelerated_times;   
    char         *raw_pcap_files;          
    char         *filter;          
    tc_pcap_file  pcap_files[MAX_PCAP_FILES]; 
    int           num_pcap_files;       
    int           conn_init_sp_fact;
    long          pcap_time;
    time_t        ignite_complete_time;
    pcap_t       *pcap;
#if (TC_PCAP_SEND)
    char         *output_if_name;
#endif
    uint16_t      rand_port_shifted;   /* random port shifted */
    uint16_t      srv_port;            /* server listening port */

    char         *raw_rs_list;         /* raw real server list */
    real_ip_addr_t  real_servers;      /* the intercept servers running intercept */
    ip_port_pair_mappings_t transfer;  /* transfered online_ip online_port
                                           target_ip target_port */
    unsigned int   port_seed;          /* port seed */
    int            multiplex_io;
    int            sig;  
    uint64_t       tries;  
    tc_event_t    *ev[MAX_FD_NUM];

    uint64_t       mem_pool_size;  
    unsigned char *mem_pool_cur_p;  
    unsigned char *mem_pool_end_p;  
    unsigned char *mem_pool;  
    unsigned char  frame[MAX_FRAME_LENGTH];
} xcopy_clt_settings;


extern int tc_raw_socket_out;
extern tc_event_loop_t event_loop;
extern xcopy_clt_settings clt_settings;

#include <tc_util.h>

#include <tc_manager.h>
#include <tc_user.h>
#include <tc_message_module.h>
#include <tc_packets_module.h>

#endif /* BURN_INCLUDED */
