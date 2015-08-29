
#include <xcopy.h>
#include <tc_user.h>

static time_t  record_time      = 0;
static time_t  last_resp_time   = 0;

static int size_of_user_index   = 0;
static int size_of_users        = 0;
static int base_user_seq        = 0;
static int relative_user_seq    = 0;

static tc_pool_t        *g_pool            = NULL;
static tc_user_t        *user_array        = NULL;
static tc_user_index_t  *user_index_array  = NULL;
static sess_table_t     *s_table           = NULL;

static bool utimer_disp(tc_user_t *u, int lty, int type);
static bool process_packet(tc_user_t *, unsigned char *, bool);
static bool process_user_packet(tc_user_t *u);
static void send_faked_ack(tc_user_t *u);
static void send_faked_rst(tc_user_t *u);
static void retransmit(tc_user_t *u, uint32_t cur_ack_seq);

static inline uint32_t 
supplemental_hash(uint32_t value) 
{
    uint32_t h;
    uint32_t tmp1 = value >> 20;
    uint32_t tmp2 = value >> 12;
    uint32_t tmp3 = tmp1 ^ tmp2;

    h = value ^ tmp3;
    tmp1 = h >> 7;
    tmp2 = h >> 4;
    tmp3 = tmp1 ^ tmp2;
    h= h ^ tmp3;

    return h;
}


static inline uint32_t 
table_index(uint32_t h, uint32_t len)
{
    return h & (len - 1);
}


int 
tc_build_sess_table(int size)
{
    g_pool = tc_create_pool(TC_DEFAULT_POOL_SIZE, 0);

    if (g_pool == NULL) {
        return TC_ERROR;
    }

    s_table = (sess_table_t *) tc_pcalloc(g_pool, sizeof(sess_table_t));
    if (s_table == NULL) {
        tc_log_info(LOG_WARN, 0, "calloc error for sess table");
        return TC_ERROR;
    }

    s_table->size = size;
    s_table->entries = (p_sess_entry *) tc_pcalloc(g_pool, 
            size * sizeof(p_sess_entry));

    if (s_table->entries == NULL) {
        tc_log_info(LOG_WARN, 0, "calloc error for sess entries");
        return TC_ERROR;
    }

    return TC_OK;
}


inline uint64_t 
tc_get_key(uint32_t ip, uint16_t port)
{
    uint64_t ip_l   = (uint64_t) ip;
    uint64_t port_l = (uint64_t) port;
    uint64_t key = (ip_l << 16) + (ip_l << 8) + port_l; 
    return key;
}


void 
tc_add_sess(p_sess_entry entry)
{
    uint32_t h = supplemental_hash((uint32_t) entry->key);
    uint32_t index = table_index(h, s_table->size);
    p_sess_entry e, last = NULL;

    for(e = s_table->entries[index]; e != NULL; e = e->next) { 
        last = e;
    } 

    if (last == NULL) {
        s_table->entries[index] = entry;
    } else {
        last->next = entry;
    }

    s_table->num_of_sess++;
    tc_log_debug2(LOG_DEBUG, 0, "index:%d,sess in table:%d",
            index, s_table->num_of_sess);
}


#if (TC_TOPO) 
static void
tc_init_sess_for_users()
{
    int             i;
    tc_user_t      *u, *prev;
    p_link_node     ln;

    if (s_table->num_of_sess == 0) {
        tc_log_info(LOG_ERR, 0, "no sess for replay");
        tc_over = 1;
        return;
    }

    ln = NULL;

    for (i = 0; i < size_of_users; i++) {
        if (ln == NULL) {
            ln = link_list_first(clt_settings.s_list);
            prev = NULL;
        }

        u = user_array + i;
        u->orig_sess = (sess_data_t *) ln->data;
        u->orig_frame = u->orig_sess->first_frame;
        u->rtt = u->orig_sess->rtt;
        u->orig_unack_frame = u->orig_sess->first_frame;

        if (prev != NULL && u->orig_sess->delayed) {
            u->state.delayed = 1;
            u->topo_prev = prev;
            tc_log_debug2(LOG_INFO, 0, "cur:%llu, parent sess:%llu",
                    u->key, u->topo_prev->key);
        }

        tc_stat.orig_clt_packs_cnt += u->orig_sess->frames;

        prev = u;
        ln = link_list_get_next(clt_settings.s_list, ln);
    }

    tc_log_info(LOG_NOTICE, 0, 
            "users:%d, sess:%d, total packets needed sent:%llu",
            size_of_users, s_table->num_of_sess, tc_stat.orig_clt_packs_cnt);
}

#else

static void
tc_init_sess_for_users()
{
    bool          is_find = false;
    int           i, index = 0;
    tc_user_t    *u;
    tc_pool_t    *pool;
    sess_data_t  *sess;
    p_sess_entry  e, *aux_s_array;

    if (s_table->num_of_sess == 0) {
        tc_log_info(LOG_ERR, 0, "no sess for replay");
        tc_over = 1;
        return;
    }

    pool = tc_create_pool(TC_DEFAULT_POOL_SIZE, 0);

    aux_s_array = (p_sess_entry *) tc_pcalloc(pool, 
            s_table->num_of_sess * sizeof(p_sess_entry));

    if (aux_s_array == NULL) {
        tc_log_info(LOG_ERR, 0, "calloc error for aux_s_array");
        tc_over = 1;
        return;
    }

    e = s_table->entries[index];

    for (i = 0; i < s_table->num_of_sess; i++) {
        if (e == NULL) {
            is_find = false;
            do {
                index = (index + 1) % (s_table->size);
                e = s_table->entries[index];
                while (e != NULL) {
                    sess = &(e->data);
                    if (!sess->has_req) {
                        e = e->next;
                    } else {
                        is_find = true;
                        break;
                    }
                }

                if (is_find) {
                    break;
                }
            } while (e == NULL);
        }
        
        aux_s_array[i] = e;
        e = e->next;
        while (e != NULL) {
            sess = &(e->data);
            if (!sess->has_req) {
                e = e->next;
            } else {
                break;
            }
        }
    }

    index = 0;
    for (i = 0; i < size_of_users; i++) {
        e = aux_s_array[index];
        u = user_array + i;
        u->orig_sess = &(e->data);
        u->orig_frame = u->orig_sess->first_frame;
        u->rtt = u->orig_sess->rtt;
        u->orig_unack_frame = u->orig_sess->first_frame;
        index = (index + 1) % s_table->num_of_sess;
        tc_stat.orig_clt_packs_cnt += u->orig_sess->frames;
    }

    tc_log_info(LOG_NOTICE, 0, 
            "users:%d, sess:%d, total packets needed sent:%llu",
            size_of_users, s_table->num_of_sess, tc_stat.orig_clt_packs_cnt);

    tc_destroy_pool(pool);
}
#endif

p_sess_entry 
tc_retrieve_sess(uint64_t key)
{
    uint32_t h = supplemental_hash((uint32_t) key);
    uint32_t index = table_index(h, s_table->size);
    p_sess_entry e, last = NULL;

    for(e = s_table->entries[index]; e != NULL; e = e->next) { 
        if (e->key == key) {   
            last = e;
        }   
    } 

    return last;
}

static tc_user_t *
tc_retr_unignite_user()
{
    int        total, speed;
    time_t     cur;
    tc_user_t *u = NULL; 

    cur = tc_time();

    if (record_time == 0) {
        record_time = cur;
    }

    if (!clt_settings.ignite_complete) {
        total = base_user_seq + relative_user_seq;
        if (total >= size_of_users) {
           tc_log_info(LOG_NOTICE, 0, "ignite completely");
           clt_settings.ignite_complete = true;
           clt_settings.ignite_complete_time = cur;
        } else {
            u = user_array + total;
            speed = clt_settings.conn_init_sp_fact;
            relative_user_seq = (relative_user_seq + 1) % speed;

            if (relative_user_seq == 0) {
                if (record_time != cur) {
                    base_user_seq += speed;
                    record_time = cur;
                    tc_log_debug0(LOG_NOTICE, 0, "change record time");
                }
            }
        }
    }

    return u;
}


tc_user_t *
tc_retrieve_user(uint64_t key)
{
    int  index, i, min, max;

    index = key % size_of_user_index;

    min = user_index_array[index].index;

    if (index != (size_of_user_index - 1)) {
        max = user_index_array[index + 1].index;
    } else {
        max = size_of_users;
    }

    tc_log_debug3(LOG_DEBUG, 0, "retrieve user,usr key :%llu,min=%d,max=%d", 
            key, min, max);
    for (i = min; i < max; i++) {
        if (user_array[i].key == key) {
            return user_array + i;
        }
    }

    return NULL;
}


static uint16_t 
get_port(int default_port)
{
    int value;

    if (!clt_settings.port_seed) {
        value = default_port;
    } else {
        value = (int) ((rand_r(&clt_settings.port_seed) / (RAND_MAX + 1.0)) * 
                VALID_PORTS_NUM);
        value += FIRST_PORT;
    }

    return htons((uint16_t) value);
}


bool 
tc_build_users(int port_prioritized, int num_users, uint32_t *ips, int num_ip)
{
    int         i, j, k, count, sub_key, slot_avg,
               *stat, *accum, *slot_cnt, *sub_keys;
    uint16_t   *buf_ports, port;
    uint32_t    ip, *buf_ips;
    uint64_t    key, *keys;
    tc_pool_t  *pool;
    
    tc_log_info(LOG_INFO, 0, "enter tc_build_users");

    pool = tc_create_pool(TC_DEFAULT_POOL_SIZE, 0);

    if (pool == NULL) {
        return false;
    }

    size_of_users = num_users;

    slot_avg = SLOT_AVG;
    if (size_of_users < slot_avg) {
        slot_avg = size_of_users;
    }

    user_array = (tc_user_t *) tc_pcalloc (g_pool, 
            size_of_users * sizeof (tc_user_t));
    if (user_array == NULL) {
        tc_log_info(LOG_WARN, 0, "calloc error for users");
        tc_destroy_pool(pool);
        return false;
    }

    size_of_user_index = size_of_users / slot_avg;
    user_index_array = (tc_user_index_t *) tc_pcalloc (g_pool, 
            size_of_user_index * sizeof(tc_user_index_t));

    if (user_array == NULL) {
        tc_log_info(LOG_WARN, 0, "calloc error for users");
        tc_destroy_pool(pool);
        return false;
    }

    count = 0;
    keys  = (uint64_t *) tc_palloc (pool, sizeof(uint64_t) * size_of_users);
    if (keys == NULL) {
        tc_destroy_pool(pool);
        return false;
    }

    sub_keys  = (int *) tc_palloc (pool, sizeof(int) * size_of_users);
    if (sub_keys == NULL) {
        tc_destroy_pool(pool);
        return false;
    }

    buf_ips   = (uint32_t *) tc_palloc (pool, sizeof(uint32_t) * size_of_users);
    if (buf_ips == NULL) {
        tc_destroy_pool(pool);
        return false;
    }

    buf_ports = (uint16_t *) tc_palloc (pool, sizeof(uint16_t) * size_of_users);
    if (buf_ports == NULL) {
        tc_destroy_pool(pool);
        return false;
    }

    accum = (int *) tc_palloc (pool, sizeof(int) * size_of_users);
    if (accum == NULL) {
        tc_destroy_pool(pool);
        return false;
    }
    
    stat = (int *) tc_palloc (pool, sizeof(int) * size_of_user_index);
    if (stat == NULL) {
        tc_destroy_pool(pool);
        return false;
    }

    slot_cnt  = (int *) tc_palloc (pool, sizeof(int) * size_of_user_index);
    if (slot_cnt == NULL) {
        tc_destroy_pool(pool);
        return false;
    }

    memset(stat, 0, sizeof(int) * size_of_user_index);
    memset(slot_cnt, 0, sizeof(int) * size_of_user_index);
    memset(sub_keys, 0, sizeof(int) * size_of_users);

    if (!port_prioritized) {
        for (j = FIRST_PORT; j <= LAST_PORT; j++) {
            port = get_port(j);
            for (i = 0; i < num_ip; i++) {
                ip = ips[i];

                key = tc_get_key(ip, port);
                if (count >= size_of_users) {
                    break;
                }

                sub_key = key % size_of_user_index;
                if (stat[sub_key] >= SLOT_MAX) {
                    continue;
                }
                buf_ips[count] = ip;
                buf_ports[count] = port;
                sub_keys[count] = sub_key;
                keys[count++] = key;
                stat[sub_key]++;
            }
        }
    } else {

        for (i = 0; i < num_ip; i++) {
            ip = ips[i];
            for (j = FIRST_PORT; j <= LAST_PORT; j++) {
                port = get_port(j);
                key = tc_get_key(ip, port);
                if (count >= size_of_users) {
                    break;
                }

                sub_key = key % size_of_user_index;
                if (stat[sub_key] >= SLOT_MAX) {
                    continue;
                }
                buf_ips[count] = ip;
                buf_ports[count] = port;
                sub_keys[count] = sub_key;
                keys[count++] = key;
                stat[sub_key]++;
            }
        }
    }

    if (count < size_of_users) {
        tc_log_info(LOG_ERR, 0, "insuffient ips:%d for creating users:%d", 
                num_ip, size_of_users);
        tc_destroy_pool(pool);
        return false;
    }

    user_index_array[0].index = 0;
    for (i = 1; i < size_of_user_index; i++) {
        user_index_array[i].index = stat[i - 1] + user_index_array[i - 1].index;
    }

    for (i = 0; i < size_of_users; i++) {
        sub_key = sub_keys[i];
        if (sub_key > 0) {
            accum[i] = user_index_array[sub_key].index  + slot_cnt[sub_key];
        } else {
            accum[i] = slot_cnt[sub_key];

        }

        k = accum[i];
        user_array[k].src_addr = buf_ips[i];
        user_array[k].src_port = buf_ports[i];
        user_array[k].key = keys[i];
        tc_log_debug2(LOG_DEBUG, 0, "usr key :%llu,pos=%d", keys[i], k);

        slot_cnt[sub_key]++;
    }

    tc_destroy_pool(pool);

    tc_init_sess_for_users();

    tc_log_info(LOG_INFO, 0, "leave tc_build_users");

    return true;
}


static inline bool 
check_final_timeout_needed(tc_user_t *u)
{
    int send_diff = tc_time() - u->last_sent_time;
    if (send_diff > 3) {
        if (utimer_disp(u, clt_settings.sess_ms_timeout, TYPE_ACT)) {
            u->state.timeout_set = 1;
            return true;
        }
    }

    return false;
}


static inline void
record_session_over(tc_user_t *u) 
{
    u->state.over = 1;

    if (!u->state.over_recorded) {
        u->state.over_recorded = 1;
        if (tc_stat.active_conn_cnt > 0) {
            tc_stat.active_conn_cnt--;
        }
    }
}


static bool
send_stop(tc_user_t *u, bool recent_sent) 
{
    long      app_resp_diff;
    uint32_t  srv_sk_buf_s;

    tc_log_debug2(LOG_DEBUG, 0, "state:%d,p:%d", 
            u->state.status, ntohs(u->src_port));

#if (TC_HIGH_PRESSURE_CHECK)
    if (u->state.rewind_af_dup_syn) {
        tc_log_info(LOG_WARN, 0, "enter send_stop af rewind:%d",
                ntohs(u->src_port));
    }
#endif

    if (u->state.over) {
        tc_log_debug1(LOG_DEBUG, 0, "sess is over:%d", ntohs(u->src_port));
        record_session_over(u);
        return true;
    }

    if (u->orig_frame == NULL) {
#if (TC_HIGH_PRESSURE_CHECK)
        if (u->state.rewind_af_dup_syn && !u->state.rewind_af_lack) {
            tc_log_info(LOG_WARN, 0, "crazy here, orig frame null :%d",
                    ntohs(u->src_port));
        }
#endif
        tc_log_debug1(LOG_DEBUG, 0, "orig frame is null :%d", 
                ntohs(u->src_port));
        return true;
    }

#if (TC_HIGH_PRESSURE_CHECK)
    if (u->state.rewind_af_dup_syn) {
        tc_log_info(LOG_WARN, 0, "check syn retrans af rewind:%d",
                ntohs(u->src_port));
    }
#endif

    if (u->state.status & SYN_SENT) {
        if (!(u->state.status & SYN_CONFIRM)) {
            if (!recent_sent && !u->state.already_retransmit_syn) {
                tc_log_debug1(LOG_DEBUG, 0, "retransmit syn:%d", 
                    ntohs(u->src_port));
                tc_stat.retransmit_syn_cnt++;
                u->state.already_retransmit_syn = 1;
                u->orig_frame = u->orig_sess->first_frame;
                return false;
            } else {
                tc_log_debug1(LOG_DEBUG, 0, "wait syn ack:%d", 
                        ntohs(u->src_port));
                return true;
            }
        }
    }

    if (u->state.status & CLIENT_FIN) {
        if (!(u->state.status & SERVER_FIN)) {
            tc_log_debug1(LOG_DEBUG, 0, "client wait server fin:%d", 
                ntohs(u->src_port));
            return true;
        }
    }

    if (u->state.resp_waiting) {
#if (TC_HIGH_PRESSURE_CHECK)
        if (u->state.rewind_af_dup_syn) {
            tc_log_info(LOG_WARN, 0, "resp waiting af rewind dup syn:%d",
                    ntohs(u->src_port));
        }
#endif
        check_final_timeout_needed(u);
        return true;
    }

    if (u->state.status & SEND_REQ) {
        if (u->orig_frame->next != NULL) {
            srv_sk_buf_s = u->orig_frame->next->seq - u->orig_frame->seq;
            srv_sk_buf_s = srv_sk_buf_s + u->orig_frame->seq - u->last_ack_seq;
            if (srv_sk_buf_s > u->srv_window) {
                tc_log_debug3(LOG_DEBUG, 0, "wait,srv_sk_buf_s:%u,win:%u,p:%u",
                        srv_sk_buf_s, u->srv_window, ntohs(u->src_port));
                return true;
            }
        }
    }

#if (TC_HIGH_PRESSURE_CHECK)
    if (u->state.rewind_af_dup_syn) {
        tc_log_info(LOG_WARN, 0, "check wait resp af rewind:%d",
                ntohs(u->src_port));
    }
#endif
    app_resp_diff = tc_milliscond_time() - u->last_recv_resp_cont_time;
    if (app_resp_diff <= 10) {
        tc_log_debug1(LOG_DEBUG, 0, "need to wait resp complete:%d", 
                ntohs(u->src_port));
        return true;
    }

    tc_log_debug1(LOG_DEBUG, 0, "last resort, set stop false:%d", 
                ntohs(u->src_port));

#if (TC_HIGH_PRESSURE_CHECK)
    if (u->state.rewind_af_dup_syn) {
        tc_log_info(LOG_WARN, 0, "stop false af rewind dup syn:%d",
                ntohs(u->src_port));
    }
#endif

    return false;
}


#if (!TC_SINGLE)
static bool
send_router_info(tc_user_t *u, uint16_t type)
{
    int               i, fd;
    bool              result = false;
    msg_client_t      msg;
    connections_t    *connections;

    memset(&msg, 0, sizeof(msg_client_t));
    msg.client_ip = u->src_addr;
    msg.client_port = u->src_port;
    msg.type = htons(type);
    msg.target_ip = u->dst_addr;
    msg.target_port = u->dst_port;

    for (i = 0; i < clt_settings.real_servers.num; i++) {

        if (!clt_settings.real_servers.active[i]) {
            continue;
        }

        connections = &(clt_settings.real_servers.connections[i]);
        fd = connections->fds[connections->index];
        connections->index = (connections->index + 1) % connections->num;

        if (fd == -1) {
            tc_log_info(LOG_WARN, 0, "sock invalid");
            continue;
        }

        if (tc_socket_send(fd, (char *) &msg, MSG_CLIENT_SIZE) == TC_ERROR) {
            tc_log_info(LOG_ERR, 0, "fd:%d, msg client send error", fd); 
            if (clt_settings.real_servers.active[i] != 0) {
                clt_settings.real_servers.active[i] = 0;
                clt_settings.real_servers.active_num--;
            }

            continue;
        }
        result = true;
    }             
    return result;
}
#endif


static void
fill_timestamp(tc_user_t *u, tc_tcph_t *tcp)
{
    uint32_t        timestamp;
    unsigned char  *opt, *p; 

    p   = (unsigned char *) tcp;
    opt = p + sizeof(tc_tcph_t);
    opt[0] = 1;
    opt[1] = 1;
    opt[2] = 8;
    opt[3] = 10;
    timestamp = htonl(u->ts_value);
    bcopy((void *) &timestamp, (void *) (opt + 4), sizeof(timestamp));
    timestamp = htonl(u->ts_ec_r);
    bcopy((void *) &timestamp, (void *) (opt + 8), sizeof(timestamp));
    tc_log_debug3(LOG_DEBUG, 0, "fill ts:%u,%u,p:%u", 
            u->ts_value, u->ts_ec_r, ntohs(u->src_port));
}


static void 
update_timestamp(tc_user_t *u, tc_tcph_t *tcp)
{
    uint32_t       ts;
    unsigned int   opt, opt_len;
    unsigned char *p, *end;

    p = ((unsigned char *) tcp) + TCP_HEADER_MIN_LEN;
    end =  ((unsigned char *) tcp) + (tcp->doff << 2);  
    while (p < end) {
        opt = p[0];
        switch (opt) {
            case TCPOPT_TIMESTAMP:
                if ((p + 1) >= end) {
                    return;
                }
                opt_len = p[1];
                if ((p + opt_len) <= end) {
                    ts = htonl(u->ts_ec_r);
                    tc_log_debug2(LOG_DEBUG, 0, "set ts reply:%u,p:%u", 
                            u->ts_ec_r, ntohs(u->src_port));
                    bcopy((void *) &ts, (void *) (p + 6), sizeof(ts));
                    ts = EXTRACT_32BITS(p + 2);
                    if (ts < u->ts_value) {
                        tc_log_debug1(LOG_DEBUG, 0, "ts < history,p:%u",
                                ntohs(u->src_port));
                        ts = htonl(u->ts_value);
                        bcopy((void *) &ts, (void *) (p + 2), sizeof(ts));
                    } else {
                        u->ts_value = ts;
                    }
                }
                return;
            case TCPOPT_NOP:
                p = p + 1; 
                break;
            case TCPOPT_EOL:
                return;
            default:
                if ((p + 1) >= end) {
                    return;
                }
                opt_len = p[1];
                if (opt_len < 2) {
                    tc_log_info(LOG_WARN, 0, "opt len:%d", opt_len);
                    return;
                }
                p += opt_len;
                break;
        }    
    }
}


#if (TC_PCAP_SEND)
static void
fill_frame(struct ethernet_hdr *hdr, unsigned char *smac, unsigned char *dmac)
{
    memcpy(hdr->ether_shost, smac, ETHER_ADDR_LEN);
    memcpy(hdr->ether_dhost, dmac, ETHER_ADDR_LEN);
    hdr->ether_type = htons(ETH_P_IP); 
}
#endif


static inline void
tc_delay_ack(tc_user_t *u)
{
    send_faked_ack(u); 
}


#if (TC_TOPO)
static bool 
could_start_sess(tc_user_t *u, int *diff) 
{
    int diff1, diff2;

    if (u->topo_prev != NULL) {
        if (u->topo_prev->state.delayed) {
            return false;
        }

        diff1 = (tc_time() - u->topo_prev->start_time) * 1000;
        diff2 = u->topo_prev->orig_sess->first_pcap_time;
        diff2 = u->orig_sess->first_pcap_time - diff2;

        if (diff1 >= diff2) {
            tc_log_debug2(LOG_INFO, 0, "cur:%llu activated, parent sess:%llu",
                    u->key, u->topo_prev->key);
            return true;
        }

        *diff = diff2 - diff1;

        tc_log_debug4(LOG_INFO, 0, "cur:%llu unact, prt:%llu, df1:%d, df2:%d",
                    u->key, u->topo_prev->key, diff1, diff2);
        return false;

    } else {
        tc_log_info(LOG_ERR, 0, "topo previous sess is null");
        return false;
    }
}


static inline bool 
tc_topo_ignite(tc_user_t *u)
{
    int diff = 0;

    if (u->state.delayed) {
        if (could_start_sess(u, &diff)) {
            u->state.delayed = 0;
            process_user_packet(u);
            return true;
        } else {
            if (diff) {
                utimer_disp(u, diff, TYPE_DELAY_IGNITE);
            } else {
                utimer_disp(u, DEF_TOPO_WAIT, TYPE_DELAY_IGNITE);
                tc_log_debug1(LOG_INFO, 0, "could not ignite:%llu", u->key);
            }
            return false;
        }
    } else {
        process_user_packet(u);
        return true;
    }
}
#endif


static void
tc_set_activate_timer(tc_user_t *u) 
{
    int lantency, send_diff;

    lantency  = u->orig_frame->time_diff;
    send_diff = tc_time() - u->last_sent_time;

    if (lantency < send_diff) {
        check_final_timeout_needed(u);
    }

    if (!u->ev.timer_set) {
        if (lantency < u->rtt) {
            lantency = u->rtt;
        }

        utimer_disp(u, lantency, TYPE_ACT);
    }
}


static void 
tc_lantency_ctl(tc_event_timer_t *ev) 
{
    uint32_t   exp_h_ack_seq;
    tc_user_t *u; 

    u = ev->data;

    if (u != NULL) {
        if (u->state.over) {
            return;
        }
        u->state.set_rto = 0;
        tc_log_debug2(LOG_INFO, 0, "timer type:%u, ev:%llu", 
                u->state.timer_type, ev); 

#if (TC_HIGH_PRESSURE_CHECK)
        if (u->state.rewind_af_dup_syn) {
            tc_log_info(LOG_WARN, 0, "dispose evt af rewind af dup syn:%d",
                    ntohs(u->src_port));
        }
#endif
        switch(u->state.timer_type) {
            case TYPE_DELAY_ACK:
                if (u->state.sess_continue) {
                    process_user_packet(u);
                    u->state.sess_continue = 0;
                } else {
                    tc_delay_ack(u); 
                }
                break;
            case TYPE_ACT:
#if (TC_HIGH_PRESSURE_CHECK)
                if (u->state.rewind_af_dup_syn) {
                    tc_log_info(LOG_WARN, 0, "activate af rewind af dup syn:%d",
                            ntohs(u->src_port));
                }
#endif
                if (!u->state.timeout_set) {
                    process_user_packet(u);
                } else {
                    send_faked_rst(u);
                    record_session_over(u);
                    tc_log_debug1(LOG_INFO, 0, "timeout session:%u", 
                            ntohs(u->src_port));
                }
                break;
            case TYPE_RTO:
#if (TC_HIGH_PRESSURE_CHECK)
                if (u->state.rewind_af_dup_syn) {
                    tc_log_info(LOG_INFO, 0, "rto af rewind af dup syn:%d",
                            ntohs(u->src_port));
                }
#endif
                if (u->state.snd_after_set_rto) {
                    if (utimer_disp(u, DEFAULT_RTO, TYPE_RTO)) {
                        u->state.snd_after_set_rto = 0;
                        tc_log_debug1(LOG_INFO, 0, "special rto:%u", 
                                ntohs(u->src_port));
                    } else {
                        tc_log_info(LOG_INFO, 0, "rto failure:%u", 
                                ntohs(u->src_port));
                    }
                } else {
                    if ((!(u->state.status & SYN_CONFIRM)) || 
                            u->state.rewind_af_dup_syn) 
                    {
#if (TC_HIGH_PRESSURE_CHECK)
                        if (u->state.rewind_af_dup_syn) {
                            tc_log_info(LOG_INFO, 0, "proc pack af dup syn:%d",
                                    ntohs(u->src_port));
                        }
#endif
                        process_user_packet(u);
                    } else if (before(u->last_ack_seq, u->exp_seq)) {
                        retransmit(u, u->last_ack_seq);
                        tc_log_debug1(LOG_INFO, 0, "rto retrans:%u", 
                                ntohs(u->src_port));
                    }    
                }

                break;
            case TYPE_DELAY_OVER:
                if (u->state.status & SERVER_FIN) {
                    send_faked_rst(u);
                }
                break;
#if (TC_TOPO)
            case TYPE_DELAY_IGNITE:
                tc_topo_ignite(u);
                break;
#endif
            default:
                tc_log_info(LOG_ERR, 0, "unknown timer type");
        }

        if (u->state.over) {
            return;
        }

        if (!u->ev.timer_set) {
            exp_h_ack_seq = ntohl(u->exp_ack_seq);
            if (after(exp_h_ack_seq, u->last_snd_ack_seq)) {
                utimer_disp(u, u->rtt, TYPE_DELAY_ACK);
                tc_log_debug1(LOG_INFO, 0, "try to set delay ack timer:%u",
                        ntohs(u->src_port));
            } else if (u->orig_frame != NULL) {
                if (!u->state.resp_waiting) {
                    if (u->state.status & SYN_CONFIRM) {
                        tc_set_activate_timer(u);
                    } else {
                        if (!u->state.already_retransmit_syn) {
                            tc_set_activate_timer(u);
                        }
                    }
                }
            }
        }
    } else {
        tc_log_info(LOG_ERR, 0, "sesson already deleted:%llu", ev); 
    } 
}


static bool 
utimer_disp(tc_user_t *u, int lty, int type)
{
    int timeout = lty > 0 ? lty:1;

    if (u->state.evt_added) {
        if (tc_event_update_timer(&u->ev, timeout)) {
            u->state.timer_type = type;
            return true;
        } else {
            tc_log_debug2(LOG_INFO, 0, "not update:%d,p:%u", timeout, 
                    ntohs(u->src_port)); 
            return false;
        }
    } else {
        u->state.evt_added = 1;
        tc_event_add_timer(NULL, &u->ev, timeout, u, tc_lantency_ctl);
        u->state.timer_type = type;
        tc_log_debug2(LOG_INFO, 0, "add timer, ev:%llu,p:%u", &u->ev, 
                    ntohs(u->src_port)); 
        return true;
    } 
}


static void
add_rto_timer_when_sending_packets(tc_user_t *u) 
{
    if (u->state.set_rto) {
        u->state.snd_after_set_rto = 1; 
    } else {
        if (utimer_disp(u, DEFAULT_RTO, TYPE_RTO)) {
            u->state.set_rto = 1; 
            u->state.snd_after_set_rto = 0; 
        } else {
            tc_log_info(LOG_INFO, 0, "set rto timer failure"); 
        }
    }
}


static bool 
process_packet(tc_user_t *u, unsigned char *frame, bool hist_record) 
{
    bool                    result;
    uint16_t                size_ip, size_tcp, tot_len, cont_len;
    uint32_t                h_ack, h_last_ack;
    tc_iph_t               *ip;
    tc_tcph_t              *tcp;
    ip_port_pair_mapping_t *test;

    ip  = (tc_iph_t *) (frame + ETHERNET_HDR_LEN);
    size_ip    = ip->ihl << 2;
    tcp = (tc_tcph_t *) ((char *) ip + size_ip);
    size_tcp = tcp->doff << 2;
    tot_len  = ntohs(ip->tot_len);
    cont_len = tot_len - size_tcp - size_ip;

    u->exp_seq = ntohl(tcp->seq);

    if (u->dst_port == 0) {
        test = get_test_pair(&(clt_settings.transfer), ip->daddr, tcp->dest);
        if (test == NULL) {
            tc_log_info(LOG_NOTICE, 0, "test null:%u", ntohs(tcp->dest));
            tc_log_trace(LOG_WARN, 0, TO_BAKEND_FLAG, ip, tcp);
            return false;
        }
        u->dst_addr = test->target_ip;
        u->dst_port = test->target_port;
#if (TC_PCAP_SEND)
        u->src_mac  = test->src_mac;
        u->dst_mac  = test->dst_mac;
#endif
    }

    if (u->state.last_ack_recorded) {
        if (u->state.status < SEND_REQ && (u->state.status & SYN_CONFIRM)) {
            h_ack = ntohl(tcp->ack_seq);
            h_last_ack = ntohl(u->history_last_ack_seq);
            if (after(h_ack, h_last_ack)) {
                tc_log_debug3(LOG_DEBUG, 0, 
                        "server resp first, wait, lack:%u, ack:%u, p:%u", 
                        h_ack, h_last_ack, ntohs(u->src_port));
                u->state.resp_waiting = 1;
                return false;
            }
        }
    }

    ip->saddr = u->src_addr;
    tcp->source = u->src_port;

    if (hist_record) {
        u->history_last_ack_seq = tcp->ack_seq;
    }

    u->state.last_ack_recorded = 1;
    tcp->ack_seq = u->exp_ack_seq;
    u->last_snd_ack_seq = ntohl(tcp->ack_seq);
    ip->daddr = u->dst_addr;
    tcp->dest = u->dst_port;

    tc_log_debug2(LOG_DEBUG, 0, "set ack seq:%u, p:%u",
            ntohl(u->exp_ack_seq), ntohs(u->src_port));

    tc_stat.packs_sent_cnt++;
    if (tcp->syn) {
        tc_log_debug2(LOG_INFO, 0, "rtt:%ld, p:%u", u->rtt, ntohs(u->src_port));
        tc_stat.syn_sent_cnt++;
        if (!(u->state.status & SYN_SENT)) {
#if (!TC_SINGLE)
            if (!send_router_info(u, CLIENT_ADD)) {
                return false;
            }
#endif
            add_rto_timer_when_sending_packets(u);
        }
#if (TC_TOPO)
        u->start_time = tc_time();
#endif
        u->state.last_ack_recorded = 0;
        u->state.status = SYN_SENT;
        if (!u->state.sess_activated) {
            u->state.sess_activated = 1;
            tc_stat.activated_sess_cnt++;
        }

    } else if (tcp->fin) {
        tc_stat.fin_sent_cnt++;
        u->state.status  |= CLIENT_FIN;
        if ((u->state.status & SESS_OVER_STATE)) {
            record_session_over(u);
        }
    } else if (tcp->rst) {
        tc_stat.rst_sent_cnt++;
        u->state.status  |= CLIENT_FIN;
        record_session_over(u);
        tc_log_debug1(LOG_DEBUG, 0, "a reset packet to back:%u",
                ntohs(u->src_port));
    }

    if (cont_len > 0) {
        u->state.rewind_af_dup_syn = 0;
        tc_stat.cont_sent_cnt++;
        u->state.status |= SEND_REQ;
        u->exp_seq = u->exp_seq + cont_len;
        add_rto_timer_when_sending_packets(u);
    } else {
        if ((u->state.status & SYN_CONFIRM) && (u->state.status < SEND_REQ)) {
            u->state.status |= SYN_ACK;
        }
    }
    if (u->state.timestamped) {
        update_timestamp(u, tcp);
    }

    tcp->check = 0;
    tcp->check = tcpcsum((unsigned char *) ip,
            (unsigned short *) tcp, (int) (tot_len - size_ip));
#if (TC_PCAP_SEND)
    ip->check = 0;
    ip->check = csum((unsigned short *) ip, size_ip);
#endif
    tc_log_debug_trace(LOG_DEBUG, 0, TO_BAKEND_FLAG, ip, tcp);

#if (!TC_PCAP_SEND)
    result = tc_raw_socket_send(tc_raw_socket_out, ip, tot_len, ip->daddr);
#else
    fill_frame((struct ethernet_hdr *) frame, u->src_mac, u->dst_mac);
    result = tc_pcap_send(frame, tot_len + ETHERNET_HDR_LEN);
#endif

    if (result == TC_OK) {
        u->last_sent_time = tc_time();
        return true;
    } else {
        tc_log_info(LOG_ERR, 0, "send error,tot_len:%d,cont_len:%d", 
                tot_len, cont_len);
#if (!TCPCOPY_PCAP_SEND)
        tc_raw_socket_out = TC_INVALID_SOCKET;
#endif
        tc_over = SIGRTMAX;
        return false;
    }
}


static bool 
process_user_packet(tc_user_t *u)
{
    unsigned char  *frame;

    if (send_stop(u, false)) {
        return false;
    }

    frame = clt_settings.frame;

    while (true) {
        if (u->orig_frame->frame_len > MAX_FRAME_LENGTH) {
            tc_log_info(LOG_NOTICE, 0, " frame length may be damaged");
        }

        memcpy(frame, u->orig_frame->frame_data, u->orig_frame->frame_len);
        process_packet(u, frame, true);
        u->total_packets_sent++;
        u->orig_frame = u->orig_frame->next;

        if (send_stop(u, true)) {
            break;
        }
        tc_log_debug1(LOG_INFO, 0, "check resp wait:%u", ntohs(u->src_port));
        if (!u->orig_frame->belong_to_the_same_req) {
            tc_log_debug2(LOG_DEBUG, 0, "user state:%d,port:%u",
                u->state.status, ntohs(u->src_port));
            if (u->state.status & SYN_CONFIRM) {
                if (!(u->state.status & SERVER_FIN)) {
                    tc_log_debug1(LOG_DEBUG, 0, "set resp wait:%u", 
                            ntohs(u->src_port));
                    u->state.resp_waiting = 1;
                } else {
                    send_faked_rst(u);
                }
            }
            break;
        } else {
            tc_log_debug1(LOG_DEBUG, 0, "the same req:%u",  ntohs(u->src_port));
        }
    }

    return true;
}


static void 
send_faked_rst(tc_user_t *u)
{
    tc_iph_t       *ip;
    tc_tcph_t      *tcp;
    unsigned char  *p, frame[FAKE_FRAME_LEN];

    memset(frame, 0, FAKE_FRAME_LEN);
    p = frame + ETHERNET_HDR_LEN;
    ip  = (tc_iph_t *) p;
    tcp = (tc_tcph_t *) (p + IP_HEADER_LEN);

    ip->version  = 4;
    ip->ihl      = IP_HEADER_LEN/4;
    ip->frag_off = htons(IP_DF); 
    ip->ttl      = 64; 
    ip->protocol = IPPROTO_TCP;
    ip->tot_len  = htons(FAKE_MIN_IP_DATAGRAM_LEN);
    ip->saddr    = u->src_addr;
    ip->daddr    = u->dst_addr;
    tcp->source  = u->src_port;
    tcp->dest    = u->dst_port;
    tcp->seq     = htonl(u->last_ack_seq);
    tcp->ack_seq = u->exp_ack_seq;
    tcp->window  = 65535; 
    tcp->ack     = 1;
    tcp->rst     = 1;
    tcp->doff    = TCP_HEADER_DOFF_MIN_VALUE;

    process_packet(u, frame, false);
}


static void 
send_faked_ack(tc_user_t *u)
{
    tc_iph_t       *ip;
    tc_tcph_t      *tcp;
    unsigned char  *p, frame[FAKE_FRAME_LEN];

    memset(frame, 0, FAKE_FRAME_LEN);
    p = frame + ETHERNET_HDR_LEN;
    ip  = (tc_iph_t *) p;
    tcp = (tc_tcph_t *) (p + IP_HEADER_LEN);

    ip->version  = 4;
    ip->ihl      = IP_HEADER_LEN/4;
    ip->frag_off = htons(IP_DF); 
    ip->ttl      = 64; 
    ip->protocol = IPPROTO_TCP;
    ip->saddr    = u->src_addr;
    ip->daddr    = u->dst_addr;
    tcp->source  = u->src_port;
    tcp->dest    = u->dst_port;
    tcp->seq     = htonl(u->last_ack_seq);
    tcp->ack_seq = u->exp_ack_seq;
    tcp->window  = 65535; 
    tcp->ack     = 1;
    if (u->state.timestamped) {
        ip->tot_len  = htons(FAKE_IP_TS_DATAGRAM_LEN);
        tcp->doff    = TCP_HEADER_DOFF_TS_VALUE;
        fill_timestamp(u, tcp);
    } else {
        ip->tot_len  = htons(FAKE_MIN_IP_DATAGRAM_LEN);
        tcp->doff    = TCP_HEADER_DOFF_MIN_VALUE;
    }

    process_packet(u, frame, false);
}


static void
retransmit(tc_user_t *u, uint32_t cur_ack_seq)
{
    frame_t        *unack_frame, *next;
    unsigned char  *frame;

    unack_frame = u->orig_unack_frame;
    if (unack_frame == NULL) {
        return;
    }

    frame = clt_settings.frame;
    next  = unack_frame->next;

    while (true) {
        if (unack_frame == u->orig_frame) {
            break;
        }

        if (!unack_frame->has_payload && next != NULL) {
            unack_frame = next;
            next = unack_frame->next;
            continue;
        }

        if (unack_frame->seq == cur_ack_seq) {
            tc_log_debug1(LOG_DEBUG, 0, "packets retransmitted:%u", 
                    ntohs(u->src_port));
            memcpy(frame, unack_frame->frame_data, unack_frame->frame_len);
            process_packet(u, frame, false);
            tc_stat.retransmit_cnt++;
            break;
        } else if (before(unack_frame->seq, cur_ack_seq) && next != NULL &&
                before(cur_ack_seq, next->seq)) 
        {
            tc_log_debug1(LOG_DEBUG, 0, "packets partly retransmitted:%u", 
                    ntohs(u->src_port));
            tc_stat.retransmit_cnt++;
            memcpy(frame, unack_frame->frame_data, unack_frame->frame_len);
            process_packet(u, frame, false);
            break;
        } else if (before(unack_frame->seq, cur_ack_seq)) {
            unack_frame = next;
            if (unack_frame == NULL) {
                break;
            }
            next = unack_frame->next;
        } else {
            tc_log_debug1(LOG_DEBUG, 0, "packets retransmitted not match:%u", 
                    ntohs(u->src_port));
            break;
        }
    }
}


static void
update_ack_packets(tc_user_t *u, uint32_t cur_ack_seq)
{
    frame_t   *unack_frame, *next;

    unack_frame = u->orig_unack_frame;
    if (unack_frame == NULL) {
        return;
    }

    if (u->orig_frame) {
        while (after(cur_ack_seq, u->orig_frame->seq)) {
            tc_log_debug1(LOG_DEBUG, 0, "rewind slide win:%u", 
                ntohs(u->src_port));
#if (TC_HIGH_PRESSURE_CHECK)
            u->state.rewind_af_lack = 1;
            if (u->state.rewind_af_dup_syn) {
                tc_log_info(LOG_INFO, 0, "rewind seq:%u, cur_ack_seq:%u p:%u", 
                        u->orig_frame->seq, cur_ack_seq, ntohs(u->src_port));
            }
#endif
            u->orig_frame = u->orig_frame->next;
            if (!u->orig_frame) {
                break;
            }
        }
    }

    next = unack_frame->next;
    while (true) {
        if (unack_frame == u->orig_frame) {
            break;
        }
        
        if (next != NULL) {
            if (next->seq == cur_ack_seq) {
                u->orig_unack_frame = unack_frame->next;
                break;
            } else if (before(cur_ack_seq, next->seq) && 
                    before(unack_frame->seq, cur_ack_seq)) 
            {
                tc_log_debug1(LOG_DEBUG, 0, "partially acked:%u", 
                        ntohs(u->src_port));
                break;
            } else {    
                tc_log_debug1(LOG_DEBUG, 0, "skipped:%u", ntohs(u->src_port));
                unack_frame = next;
                next = unack_frame->next;
                if (unack_frame == u->orig_sess->last_frame) {
                    break;
                }
            }
        } else {
            if (before(unack_frame->seq, cur_ack_seq)) {
                unack_frame = unack_frame->next;
            }
            u->orig_unack_frame = unack_frame;
            break;
        }
    }

}


static void         
retrieve_options(tc_user_t *u, int direction, tc_tcph_t *tcp)
{                   
    uint32_t       ts_value; 
    unsigned int   opt, opt_len;
    unsigned char *p, *end;

    p = ((unsigned char *) tcp) + TCP_HEADER_MIN_LEN;
    end =  ((unsigned char *) tcp) + (tcp->doff << 2);  
    while (p < end) {
        opt = p[0];
        switch (opt) {
            case TCPOPT_WSCALE:
                if ((p + 1) >= end) {
                    return;
                }
                opt_len = p[1];
                if ((p + opt_len) > end) {
                    return;
                }
                u->wscale = (uint16_t) p[2];
                p += opt_len;
            case TCPOPT_TIMESTAMP:
                if ((p + 1) >= end) {
                    return;
                }
                opt_len = p[1];
                if ((p + opt_len) > end) {
                    return;
                }
                if (direction == LOCAL) {
                    ts_value = EXTRACT_32BITS(p + 2);
                } else {
                    u->ts_ec_r  = EXTRACT_32BITS(p + 2);
                    ts_value = EXTRACT_32BITS(p + 6);
                    if (tcp->syn) {
                        u->state.timestamped = 1;
                        tc_log_debug1(LOG_DEBUG, 0, "timestamped,p=%u", 
                                ntohs(u->src_port));
                    }
                    tc_log_debug3(LOG_DEBUG, 0, 
                            "get ts(client viewpoint):%u,%u,p:%u", 
                            u->ts_value, u->ts_ec_r, ntohs(u->src_port));
                }
                if (ts_value > u->ts_value) {
                    tc_log_debug1(LOG_DEBUG, 0, "ts > history,p:%u",
                            ntohs(u->src_port));
                    u->ts_value = ts_value;
                }
                p += opt_len;
            case TCPOPT_NOP:
                p = p + 1; 
                break;                      
            case TCPOPT_EOL:
                return;
            default:
                if ((p + 1) >= end) {
                    return;
                }
                opt_len = p[1];
                p += opt_len;
                break;
        }    
    }

    return;
}


void 
process_outgress(unsigned char *packet)
{
    uint16_t     size_ip, size_tcp, tot_len, cont_len;
    uint32_t     seq, ack_seq, cur_target_ack_seq, last_target_ack_seq;
    uint64_t     key;
    tc_user_t   *u;
    tc_iph_t    *ip;
    tc_tcph_t   *tcp;

    last_resp_time = tc_time();
    tc_stat.resp_cnt++;
    ip  = (tc_iph_t *) packet;
    size_ip    = ip->ihl << 2;
    tcp = (tc_tcph_t *) ((char *) ip + size_ip);

    key = tc_get_key(ip->daddr, tcp->dest);
    tc_log_debug1(LOG_DEBUG, 0, "key from bak:%llu", key);
    u = tc_retrieve_user(key);

    if (u != NULL) {

        if (!(u->state.status & SYN_SENT)) {
            tc_log_info(LOG_NOTICE, 0, "abnormal resp packets");
            return;
        }

        tc_log_debug_trace(LOG_DEBUG, 0, BACKEND_FLAG, ip, tcp);
        u->srv_window = ntohs(tcp->window);
        if (u->wscale) {
            u->srv_window = u->srv_window << (u->wscale);
            tc_log_debug1(LOG_DEBUG, 0, "window size:%u", u->srv_window);
        }
        if (u->state.timestamped) {
            retrieve_options(u, REMOTE, tcp);
        }
        size_tcp = tcp->doff << 2;
        tot_len  = ntohs(ip->tot_len);
        cont_len = tot_len - size_tcp - size_ip;

        if (ip->daddr != u->src_addr || tcp->dest != u->src_port) {
            tc_log_info(LOG_NOTICE, 0, "key conflict");
        }
        seq = ntohl(tcp->seq);
        ack_seq = ntohl(tcp->ack_seq);

        if (u->last_seq == seq && u->last_ack_seq == ack_seq) {
            u->fast_retransmit_cnt++;
            if (u->fast_retransmit_cnt == 3) {
                tc_log_debug1(LOG_INFO, 0, "fast retrans:%u", 
                            ntohs(u->src_port));
                retransmit(u, ack_seq);
                return;
            }
        } else {
            if (u->last_ack_seq != ack_seq) {
                update_ack_packets(u, ack_seq);
            }
            u->fast_retransmit_cnt = 0;
        }

        u->last_ack_seq =  ack_seq;
        u->last_seq =  seq;

        if (cont_len > 0) {
            u->last_recv_resp_cont_time = tc_milliscond_time();
            tc_stat.resp_cont_cnt++;
            u->state.resp_waiting = 0;   
            cur_target_ack_seq = seq + cont_len;
            last_target_ack_seq = ntohl(u->exp_ack_seq);
            if (after(cur_target_ack_seq, last_target_ack_seq) || tcp->fin) {
                u->exp_ack_seq = htonl(cur_target_ack_seq);
                utimer_disp(u, u->rtt, TYPE_DELAY_ACK);
            } else {
                u->rtt = u->rtt >> 1;
                tc_log_debug2(LOG_INFO, 0, "retrans bf fin, rtt:%ld,p:%u", 
                        u->rtt, ntohs(u->src_port));
            }
        } else {
            u->exp_ack_seq = tcp->seq;
        }
        
        if (tcp->syn) {
            tc_log_debug1(LOG_DEBUG, 0, "recv syn from back:%u", 
                    ntohs(u->src_port));
            u->exp_ack_seq = htonl(ntohl(u->exp_ack_seq) + 1);
            if (!u->state.resp_syn_received) {
                tc_stat.conn_cnt++;
                tc_stat.active_conn_cnt++;
                u->state.resp_syn_received = 1;
                u->state.status |= SYN_CONFIRM;
                tc_log_debug2(LOG_DEBUG, 0, "exp ack seq:%u, p:%u",
                        ntohl(u->exp_ack_seq), ntohs(u->src_port));
                if (size_tcp > TCP_HEADER_MIN_LEN) {
                    retrieve_options(u, REMOTE, tcp);
                    if (u->wscale > 0) {
                        tc_log_debug2(LOG_DEBUG, 0, "wscale:%u, p:%u",
                                u->wscale, ntohs(u->src_port));
                    }
                }
                u->state.sess_continue = 1;
                utimer_disp(u, u->rtt, TYPE_DELAY_ACK);

            } else {
                tc_log_debug1(LOG_DEBUG, 0, "dup syn:%u", ntohs(u->src_port));
                if (u->state.status & SYN_ACK) {
                    tc_log_debug1(LOG_DEBUG, 0, "third had been sent:%u",
                        ntohs(u->src_port));
                    u->state.rewind_af_dup_syn = 1;
#if (TC_HIGH_PRESSURE_CHECK)
                    tc_log_info(LOG_INFO, 0, "rewind to fir payload frame:%u",
                            ntohs(u->src_port));
#endif
                    send_faked_ack(u);
                    u->orig_frame = u->orig_sess->first_payload_frame;
                    utimer_disp(u, u->rtt, TYPE_ACT);
                }
            }
        } else if (tcp->fin) {
#if (TC_HIGH_PRESSURE_CHECK && TC_COMET)
            tc_log_info(LOG_INFO, 0, "recv fin from back:%u", 
                    ntohs(u->src_port));
#else
            tc_log_debug1(LOG_DEBUG, 0, "recv fin from back:%u", 
                    ntohs(u->src_port));
#endif
            u->exp_ack_seq = htonl(ntohl(u->exp_ack_seq) + 1);
            u->state.status  |= SERVER_FIN;
            if (u->state.status & CLIENT_FIN) {
                send_faked_ack(u);
                record_session_over(u);
            } else {
#if (TC_COMET)
                send_faked_ack(u);
                send_faked_rst(u);
#else
                if (u->orig_frame == NULL) {
                    send_faked_rst(u);
                } else {
                    process_user_packet(u);
                    utimer_disp(u, FIN_TIMEOUT, TYPE_DELAY_OVER);
                }
#endif
            }
            tc_stat.fin_recv_cnt++;

        } else if (tcp->rst) {
            tc_log_info(LOG_NOTICE, 0, "recv rst from back:%u", 
                    ntohs(u->src_port));
            tc_stat.rst_recv_cnt++;
            if (u->state.status == SYN_SENT) {
                if (!u->state.over) {
                    tc_stat.conn_reject_cnt++;
                }
            }

            u->state.status  |= SERVER_FIN;
            record_session_over(u);
        }

    } else {
        tc_log_debug_trace(LOG_DEBUG, 0, BACKEND_FLAG, ip, tcp);
        tc_log_debug0(LOG_DEBUG, 0, "no active sess for me");
    }
}


static inline void 
check_replay_complete()
{
    int  diff;

    if (tc_stat.activated_sess_cnt >= size_of_users) {

        if (last_resp_time) {
            diff = tc_time() - last_resp_time;
        } else {
            diff = tc_time() - record_time;
        }

        if (diff > DEFAULT_TIMEOUT) {
#if (TC_COMET)
            if (tc_stat.resp_cnt == 0) {
                tc_over = 1;
            }
#else
            tc_over = 1;
#endif
        }
    }
}


void
ignite_one_sess()
{
    tc_user_t  *u;

    u = tc_retr_unignite_user();

    if (u != NULL) {
        tc_log_debug1(LOG_INFO, 0, "ingress user:%llu", u->key);
#if (TC_TOPO)
        tc_topo_ignite(u);
#else
        process_user_packet(u);
#endif
    }
}


void
output_stat()
{
    tc_log_info(LOG_NOTICE, 0, "active conns:%d", tc_stat.active_conn_cnt);
    tc_log_info(LOG_NOTICE, 0, "reject:%d, reset recv:%d,fin recv:%d",
            tc_stat.conn_reject_cnt, tc_stat.rst_recv_cnt, 
            tc_stat.fin_recv_cnt);
    tc_log_info(LOG_NOTICE, 0, "reset sent:%d, fin:%d",
            tc_stat.rst_sent_cnt, tc_stat.fin_sent_cnt);
    tc_log_info(LOG_NOTICE, 0, "retrans:%llu, syn retrans:%d",
            tc_stat.retransmit_cnt, tc_stat.retransmit_syn_cnt);
    tc_log_info(LOG_NOTICE, 0, "conns:%d,resp packs:%llu,c-resp packs:%llu",
            tc_stat.conn_cnt, tc_stat.resp_cnt, tc_stat.resp_cont_cnt);
    tc_log_info(LOG_NOTICE, 0, 
            "syn sent cnt:%d,clt packs sent:%llu,clt cont sent:%llu",
            tc_stat.syn_sent_cnt, tc_stat.packs_sent_cnt, 
            tc_stat.cont_sent_cnt);
}


void
tc_interval_dispose(tc_event_timer_t *evt)
{
    output_stat();

    check_replay_complete();

    if (!tc_over) {
        tc_event_update_timer(evt, 5000);
    }
}


void 
release_user_resources()
{
    int                 i, j, k, m, diff, rst_snd_cnt = 0, valid_s = 0, thrsh;
    tc_user_t          *u;
    p_sess_entry        e;
    struct sockaddr_in  targ_addr;

    memset(&targ_addr, 0, sizeof(targ_addr));
    targ_addr.sin_family = AF_INET;

    if (s_table && s_table->num_of_sess > 0) {
        if (user_array) {
            j = 0;
            k = 0;
            m = 0;
            thrsh = SLP_THRSH_NUM;
            for (i = 0; i < size_of_users; i++) {
                u = user_array + i;
                
                if (!u->orig_sess) {
                    continue;
                }

                if (!(u->state.status & SYN_CONFIRM)) {
                    m++;
                    if (m < 1024) {
                        targ_addr.sin_addr.s_addr = u->src_addr;
                        tc_log_info(LOG_NOTICE, 0, "conn fails:%s:%u", 
                                inet_ntoa(targ_addr.sin_addr), 
                                ntohs(u->src_port));
                    }
                }
                   
                diff = u->orig_sess->frames - u->total_packets_sent;
                if (diff > 0) {
                    tc_log_debug3(LOG_DEBUG, 0, 
                            "total sent frames:%u, total:%u, p:%u", 
                            u->total_packets_sent, u->orig_sess->frames, 
                            ntohs(u->src_port));
                    if (diff > 1) {
                        tc_log_debug2(LOG_NOTICE, 0, "lost packs:%d, p:%u", 
                            diff, ntohs(u->src_port));
                    }
                }
                if (u->state.status && !u->state.over) {
#if (!TC_COMET && TC_HIGH_PRESSURE_CHECK)
                    tc_log_info(LOG_NOTICE, 0, "sess state:%u,p:%u", 
                            u->state.status, ntohs(u->src_port));
#endif
                    send_faked_rst(u);
                    j++;
                    k++;
                    if (j == thrsh) {
                        usleep(100);
                        j = 0;
                    }

                    if (k == BREAK_NUM) {
                        tc_time_update();
                        tc_log_info(LOG_INFO, 0, "sleep a sec when recycling");
                        sleep(1);
                        k = 0;
                    }
                    rst_snd_cnt++;
                }

                if (size_of_users >= MILLION) {
                    if (i == HALF_MILLION) {
                        thrsh = thrsh >> 1;
                        thrsh = thrsh ? thrsh : 1;
                    } else if (i == MILLION) {
                        thrsh = 1;
                    }
                }
            }
             
            tc_time_update();
            tc_log_info(LOG_INFO, 0, "last sleep a sec when recycling");
            sleep(1);
        }

        tc_log_info(LOG_NOTICE, 0, "send %d resets to release tcp resources", 
                rst_snd_cnt);
    }

    if (s_table) {
        for (i = 0; i < s_table->size; i++) {
            e = s_table->entries[i];
            while (e) {
                if (e->data.has_req) {
                    valid_s++;
                }
                e = e->next;
            }
        }

        tc_log_info(LOG_NOTICE, 0, "valid sess:%d", valid_s);
    }

    s_table = NULL;
    user_array = NULL;
    user_index_array = NULL;

    if (g_pool != NULL) {
        tc_destroy_pool(g_pool);
        g_pool = NULL;
    }
}

