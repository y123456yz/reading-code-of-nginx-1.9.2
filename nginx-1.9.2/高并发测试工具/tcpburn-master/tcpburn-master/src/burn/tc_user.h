#ifndef  TC_USER_INCLUDED
#define  TC_USER_INCLUDED

#include <xcopy.h>
#include <burn.h>

typedef struct frame_s {
    struct frame_s *next;
    struct frame_s *prev;
    unsigned char  *frame_data;
    uint32_t        seq;
    unsigned int    time_diff:20;
    unsigned int    belong_to_the_same_req:1;
    unsigned int    has_payload:1;
    unsigned int    frame_len:17;
}frame_t;

typedef struct sess_data_s {
    frame_t *first_frame;
    frame_t *first_payload_frame;
    frame_t *last_frame;
    long     first_pcap_time;
    long     last_pcap_time;
    long     rtt;
    uint32_t last_ack_seq;
    uint32_t frames;
    unsigned int rtt_init:1;
    unsigned int rtt_calculated:1;
    unsigned int end:1;
#if (TC_TOPO)
    unsigned int delayed:1;
#endif
    unsigned int has_req:1;
    unsigned int status:10;
}sess_data_t, *p_sess_data_t;

typedef struct sess_entry_s {
    uint64_t key;
    sess_data_t data;
    struct sess_entry_s *next;
}sess_entry_t,*p_sess_entry;

typedef struct sess_table_s { 
    int size;
    int num_of_sess;
    p_sess_entry *entries;
}sess_table_t;


typedef struct tc_user_state_s {
    uint32_t status:16;
    uint32_t timer_type:4;
#if (TC_TOPO)
    uint32_t delayed:1;
#endif
    uint32_t over:1;
    uint32_t over_recorded:1;
    uint32_t timestamped:1;
    uint32_t resp_syn_received:1;
    uint32_t resp_waiting:1;
    uint32_t sess_activated:1;
    uint32_t sess_continue:1;
    uint32_t last_ack_recorded:1;
    uint32_t evt_added:1;
    uint32_t set_rto:1;
    uint32_t already_retransmit_syn:1;
    uint32_t timeout_set:1;
    uint32_t snd_after_set_rto:1;
    uint32_t rewind_af_dup_syn:1;
#if (TC_HIGH_PRESSURE_CHECK)
    uint32_t rewind_af_lack:1;
#endif
}tc_user_state_t;


typedef struct tc_user_s {
    uint64_t key;
    tc_user_state_t  state;

    uint32_t orig_clt_addr;
    uint32_t src_addr;
    uint32_t dst_addr;

    uint16_t orig_clt_port;
    uint16_t src_port;
    uint16_t dst_port;

    uint16_t wscale;
    uint32_t last_seq;             /* network byte order */
    uint32_t last_ack_seq;         /* host byte order */
    uint32_t history_last_ack_seq; /* network byte order */
    uint32_t exp_seq;              /* host byte order */
    uint32_t exp_ack_seq;          /* network byte order */
    uint32_t last_snd_ack_seq;     /* host byte order */ 
    
    uint32_t fast_retransmit_cnt:6;

    uint32_t ts_ec_r;
    uint32_t ts_value; 

    uint32_t srv_window;
    uint32_t total_packets_sent;

#if (TC_PCAP_SEND)
    unsigned char *src_mac;
    unsigned char *dst_mac;
#endif

    tc_event_timer_t ev;

    sess_data_t *orig_sess;
    frame_t     *orig_frame;
    frame_t     *orig_unack_frame;
    long         last_recv_resp_cont_time;
    long         rtt;
#if (TC_TOPO)
    struct tc_user_s *topo_prev;
    time_t       start_time;
#endif
    time_t       last_sent_time;

}tc_user_t;

typedef struct tc_user_index_s {
    int index;
}tc_user_index_t;

typedef struct tc_stat_s {
    int      activated_sess_cnt; 
    int      fin_sent_cnt; 
    int      rst_sent_cnt; 
    int      conn_cnt; 
    int      conn_reject_cnt; 
    int      rst_recv_cnt; 
    int      fin_recv_cnt; 
    int      active_conn_cnt; 
    int      syn_sent_cnt; 
    int      retransmit_syn_cnt; 
    uint64_t retransmit_cnt; 
    uint64_t resp_cnt; 
    uint64_t resp_cont_cnt; 
    uint64_t packs_sent_cnt; 
    uint64_t cont_sent_cnt; 
    uint64_t orig_clt_packs_cnt; 
}tc_stat_t;

extern tc_stat_t   tc_stat;

int tc_build_sess_table(int size);
bool tc_build_users(int port_prioritized, int num_users, uint32_t *ips,
        int num_ip);

uint64_t tc_get_key(uint32_t ip, uint16_t port);
tc_user_t *tc_retrieve_user(uint64_t key);
void tc_add_sess(p_sess_entry entry);
p_sess_entry tc_retrieve_sess(uint64_t key);

void process_outgress(unsigned char *packet);
void ignite_one_sess();
void output_stat(); 
void tc_interval_dispose(tc_event_timer_t *evt);
void release_user_resources();

#endif   /* ----- #ifndef TC_USER_INCLUDED ----- */

