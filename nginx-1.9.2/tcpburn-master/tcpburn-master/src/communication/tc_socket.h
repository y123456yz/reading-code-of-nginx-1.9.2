#ifndef TC_SOCKET_INCLUDED
#define TC_SOCKET_INCLUDED

#define TC_INVALID_SOCKET -1

#include <xcopy.h>

#define tc_socket_close(fd) close(fd)
#define tc_socket_accept(fd) accept(fd, NULL, NULL) 

int tc_raw_socket_out_init();
int tc_raw_socket_send(int fd, void *buf, size_t len, uint32_t ip);

#if (TC_PCAP_SEND)
int tc_pcap_send_init(char *if_name, int mtu);
int tc_pcap_send(unsigned char *frame, size_t len);
int tc_pcap_over();
#endif

int tc_socket_init();
int tc_socket_set_nonblocking(int fd);
int tc_socket_set_nodelay(int fd);
int tc_socket_connect(int fd, uint32_t ip, uint16_t port);
int tc_socket_cmb_recv(int fd, int *num, char *buffer);
int tc_socket_send(int fd, char *buffer, int len);

#endif /* TC_SOCKET_INCLUDED */

