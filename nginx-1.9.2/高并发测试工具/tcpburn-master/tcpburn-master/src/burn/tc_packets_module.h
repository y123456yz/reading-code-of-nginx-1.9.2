#ifndef TC_PACKETS_MODULE_INCLUDED
#define TC_PACKETS_MODULE_INCLUDED

#include <xcopy.h>
#include <burn.h>

int tc_send_init(tc_event_loop_t *event_loop);
void read_packets_from_pcap(char *pcap_file, char *filter);
int calculate_mem_pool_size(char *pcap_file, char *filter);
void set_topo_for_sess();

#endif /* TC_PACKETS_MODULE_INCLUDED */
