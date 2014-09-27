#ifndef HAVE_INADDRT
typedef unsigned long int in_addr_t;
#define HAVE_INADDRT 1
#endif

/* get interfaces information into structure in arp.c */
void get_ifaces(void);

/* process arp reply */
int recv_arp_reply(host_data * host_info);

/* send ARP request to specified IP */
int send_arp_req(int sock, in_addr_t ip);
