 
#include <xcopy.h>

int
tc_raw_socket_out_init()
{
    int fd, n;

    n = 1;

    /*
     * On Linux when setting the protocol as IPPROTO_RAW,
     * then by default the kernel sets the IP_HDRINCL option and 
     * thus does not prepend its own IP header. 
     */
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

    if (fd == -1) {
        tc_log_info(LOG_ERR, errno, "Create raw socket to output failed");
        return TC_INVALID_SOCKET;
    } 

    /*
     * tell the IP layer not to prepend its own header.
     * It does not need setting for linux, but *BSD needs
     */
    if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &n, sizeof(n)) < 0) {
        tc_log_info(LOG_ERR, errno,
                    "Set raw socket(%d) option \"IP_HDRINCL\" failed", fd);
        return TC_INVALID_SOCKET;
    }


    return fd;
}

#if (TC_PCAP_SEND)

static pcap_t *pcap = NULL;

int
tc_pcap_send_init(char *if_name, int mtu)
{
    char  pcap_errbuf[PCAP_ERRBUF_SIZE];

    pcap_errbuf[0] = '\0';
    pcap = pcap_open_live(if_name, mtu + sizeof(struct ethernet_hdr), 
            0, 0, pcap_errbuf);
    if (pcap_errbuf[0] != '\0') {
        tc_log_info(LOG_ERR, errno, "pcap open %s, failed:%s", 
                if_name, pcap_errbuf);
        return TC_ERROR;
    }

    return TC_OK;
}

int
tc_pcap_send(unsigned char *frame, size_t len)
{
    int   send_len;
    char  pcap_errbuf[PCAP_ERRBUF_SIZE];

    pcap_errbuf[0]='\0';

    send_len = pcap_inject(pcap, frame, len);
    if (send_len == -1) {
        return TC_ERROR;
    }

    return TC_OK;
}

int tc_pcap_over()
{
    if (pcap != NULL) {
        pcap_close(pcap);
        pcap = NULL;
    }
     
    return TC_OK;
}
#endif

/*
 * send the ip packet to the remote test server
 * (It will not go through ip fragmentation)
 */

int
tc_raw_socket_send(int fd, void *buf, size_t len, uint32_t ip)
{
    ssize_t             send_len, offset = 0, num_bytes;
    const char         *ptr;
    struct sockaddr_in  dst_addr;

    if (fd > 0) {

        memset(&dst_addr, 0, sizeof(struct sockaddr_in));

        dst_addr.sin_family = AF_INET;
        dst_addr.sin_addr.s_addr = ip;

        ptr = buf;

        /*
         * The output packet will take a special path of IP layer
         * (raw_sendmsg->raw_send_hdrinc->NF_INET_LOCAL_OUT->...).
         * No IP fragmentation will take place if needed. 
         * This means that a raw packet larger than the MTU of the 
         * interface will probably be discarded. Instead ip_local_error(), 
         * which does general sk_buff cleaning, is called and an 
         * error EMSGSIZE is returned. 
         */
        do {
            num_bytes = len - offset;
            send_len = sendto(fd, ptr + offset, num_bytes, 0, 
                    (struct sockaddr *) &dst_addr, sizeof(dst_addr));

            if (send_len == -1) {
                if (errno == EINTR) {
                    tc_log_info(LOG_NOTICE, errno, "raw fd:%d EINTR", fd);
                } else if (errno == EAGAIN) {
                    tc_log_debug1(LOG_NOTICE, errno, "raw fd:%d EAGAIN", fd);
                } else {
                    tc_log_info(LOG_ERR, errno, "raw fd:%d", fd);
                    tc_socket_close(fd);
                    return TC_ERROR;
                }   

            }  else {
                offset += send_len;
            }   
        } while (offset < (ssize_t) len);
    }

    return TC_OK;
}


int
tc_socket_init()
{
    int fd;
   
    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        tc_log_info(LOG_ERR, errno, "Create socket failed");
        return TC_INVALID_SOCKET;
    }

    return fd;
}

int
tc_socket_set_nonblocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return TC_ERROR;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return TC_ERROR;
    }

    return TC_OK;
}

int
tc_socket_set_nodelay(int fd)
{
    int       flag;
    socklen_t len;

    flag = 1;
    len = (socklen_t) sizeof(flag);

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, len) == -1) { 
        return TC_ERROR;
    } 

    return TC_OK;
}

int
tc_socket_connect(int fd, uint32_t ip, uint16_t port)
{
    socklen_t           len;
    struct sockaddr_in  remote_addr;                           

    tc_memzero(&remote_addr, sizeof(remote_addr));               

    remote_addr.sin_family = AF_INET;                         
    remote_addr.sin_addr.s_addr = ip;                
    remote_addr.sin_port = htons(port);                       

    len = (socklen_t) (sizeof(remote_addr));

    if (connect(fd, (struct sockaddr *) &remote_addr, len) == -1) {
        tc_log_info(LOG_ERR, errno, "Can not connect to remote server(%s:%d)",
                inet_ntoa(remote_addr.sin_addr), port);
        tc_socket_close(fd);
        return TC_ERROR;
    } else {
        tc_log_info(LOG_INFO, 0, "connect to remote server(%s:%d)",
                inet_ntoa(remote_addr.sin_addr), port);
        return TC_OK;
    }

}


int
tc_socket_cmb_recv(int fd, int *num, char *buffer)
{
    int     read_num = 0, cnt = 0;
    size_t  last;
    ssize_t n, len;

    last = 0;
    len = sizeof(uint16_t);

    for ( ;; ) {
        n = recv(fd, buffer + last, len, 0);

        if (n == -1) {
            if (errno == EAGAIN || errno == EINTR) {
                cnt++;
                if (cnt % MAX_READ_LOG_TRIES == 0) {
                    tc_log_info(LOG_ERR, 0, "recv tries:%d,fd:%d", 
                            cnt, fd);
                }
                continue;
            } else {
                tc_log_info(LOG_NOTICE, errno, "return -1,fd:%d", fd);
                return TC_ERROR;
            }
        }

        if (n == 0) {
            tc_log_info(LOG_NOTICE, 0, "recv length 0,fd:%d", fd);
            return TC_ERROR;
        }

        last += n;

        tc_log_debug1(LOG_DEBUG, 0, "current len:%d", len);
        if ((!read_num) && last >= sizeof(uint16_t)) {
            *num = (int) ntohs(*(uint16_t *) buffer);
            if (*num > COMB_MAX_NUM) {
                tc_log_info(LOG_WARN, 0, "num:%d larger than threshold", *num);
                return TC_ERROR;
            }
            read_num = 1;
            len = ((*num) * MSG_SERVER_SIZE) + len;
            tc_log_debug2(LOG_DEBUG, 0, "all bytes needed reading:%d,num:%d",
                    len, *num);
        }

        tc_log_debug1(LOG_DEBUG, 0, "this time reading:%d", n);
        if ((len -= n) == 0) {
            tc_log_debug1(LOG_DEBUG, 0, "remain readed:%d", len);
            break;
        }

        if (len < 0) {
            tc_log_info(LOG_WARN, 0, "read:%d,num packs:%d, remain:%d",
                    n, *num, len);
            break;
        }
        tc_log_debug1(LOG_DEBUG, 0, "remain readed:%d", len);
    }

    return TC_OK;
}

int
tc_socket_send(int fd, char *buffer, int len)
{
    int         cnt = 0;
    ssize_t     send_len, offset = 0, num_bytes;
    const char *ptr;

    if (len <= 0) {
        return TC_OK;
    }

    ptr = buffer;
    num_bytes = len - offset;

    do {

        send_len = send(fd, ptr + offset, num_bytes, 0);

        if (send_len == -1) {

            if (cnt > MAX_WRITE_TRIES) {
                tc_log_info(LOG_ERR, 0, "send timeout,fd:%d", fd);
                return TC_ERROR;
            }
            if (errno == EINTR) {
                tc_log_info(LOG_NOTICE, errno, "fd:%d EINTR", fd);
                cnt++;
            } else if (errno == EAGAIN) {
                tc_log_debug1(LOG_NOTICE, errno, "fd:%d EAGAIN", fd);
                cnt++;
            } else {
                tc_log_info(LOG_ERR, errno, "fd:%d", fd);
                return TC_ERROR;
            }
        } else {

            if (send_len != num_bytes) {
                tc_log_info(LOG_WARN, 0, "fd:%d, slen:%ld, bsize:%ld",
                        fd, send_len, num_bytes);
            }

            offset += send_len;
            num_bytes -= send_len;
        }
    } while (offset < len);

    return TC_OK;
}

