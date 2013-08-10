/*

This code is original to:

(Arp) Pinger v2 21092005
Authors:	
    Tomasz Chilinski (chilek(at)chilan(dot)com
		Rafal Czyzewski (raphag(at)chilan(dot)com
		Ewa Martyniuk eveem(at)sav(dot)tkb(dot)net(dot)pl
License:	GPL

Modified for pinger (version 0.31+): 
    Petr Koloros silk(at)sinus.cz     
*/

#include <stdio.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_arp.h>
#include <features.h>    /* for the glibc version number */

#ifndef ETHER_H
#define ETHER_H
#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>     /* the L2 protocols */
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#endif
#endif

#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

#include "timefunc.h"
#include "globals.h"
#include "ping.h"
#include "arp.h"
//#include "log.h" - only for debugging

#define KB 1024

sig_atomic_t sigint = 0;

//typedef unsigned long int in_addr_t;

struct sectionstruct {
	char name[80];
	in_addr_t ip[256];
	int ips[256];
	int count;
};

struct sectionlist {
	struct sectionstruct *section;
	struct sectionlist *next;
};

struct sectionlist *first = NULL;

struct if_desc {
	int index;
	unsigned int ip;
	unsigned int netmask;
	unsigned char mac[6];
	unsigned int network;
};

//FIXME: only 8 interfaces?
struct if_desc descs[8];
int descs_count = 0;

void sig_int(int a) {
	sigint = 1;
}


void get_iface_desc(char if_name[IFNAMSIZ], struct if_desc *desc) {
	int sock;
	struct ifreq interf; //man 7 netdevice

	if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP))) == -1) {
		printf("get_iface_desc: socket: %s\n\n", strerror(errno));
		exit(1);
		}

	memset(interf.ifr_name, 0, IFNAMSIZ);
	memcpy(interf.ifr_name, if_name, strlen(if_name));

	if (ioctl(sock, SIOCGIFINDEX, &interf) == -1) {
		printf("get_iface_desc: ioctl (SIOCGIFINDEX): %s\n", strerror(errno));
		exit(1);
		}
	desc->index = interf.ifr_ifindex;   //(*par).index

	memset(interf.ifr_hwaddr.sa_data, 0, 14);
	if (ioctl(sock, SIOCGIFHWADDR, &interf) == -1) {
		printf("get_iface_desc: ioctl (SIOCGIFHWADDR): %s\n", strerror(errno));
		exit(1);
		}

	memcpy(desc->mac, interf.ifr_hwaddr.sa_data, 6);

	if (ioctl(sock, SIOCGIFADDR, &interf)) {
		printf("get_iface_desc: ioctl (SIOCGIFADDR): %s\n", strerror(errno));
		exit(1);
		}

	memcpy(&(desc->ip), (interf.ifr_addr.sa_data + 2), 4);

	if (ioctl(sock, SIOCGIFNETMASK, &interf)) {
		printf("get_iface_desc: ioctl (SIOCGIFNETMASK) %s\n", strerror(errno));
		exit(1);
		}

	memcpy(&(desc->netmask), interf.ifr_addr.sa_data + 2, 4);

	desc->network = desc->ip & desc->netmask;

	close(sock);
}


void get_ifaces(void) {
	struct ifconf ifc;
	int sock, a, i, cnt;
	struct ifreq *ifr;

	if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP))) == -1) {
		printf("print_ifaces: socket: %s\n", strerror(errno));
		exit(1);
		}
	ifc.ifc_len = 1024;
	ifc.ifc_buf = malloc(1024);
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		printf("print_ifaces: ioctl (SIOCGIFCONF): %s\n", strerror(errno));
		exit(1);
		}
	ifr = ifc.ifc_req;
	cnt = ifc.ifc_len / sizeof(struct ifreq);
	for (a = 0; a < cnt; a++) {
		get_iface_desc(ifr->ifr_name, &descs[descs_count]);
		for (i = 0; i < descs_count; i++)
			if (descs[i].network == descs[descs_count].network)
				break;
		if (i == descs_count) descs_count++;
		if (descs_count > (sizeof(descs)/sizeof(descs[0]))) {
			fprintf(stderr, "%s:%s:%d %s\n", __FILE__, __FUNCTION__, __LINE__,
                                _("sorry, maximum supported number of interfaces exhausted."));
			printf("%s\n", _("contact the author!"));
			exit(1);
		}
		ifr++;
	}
	free(ifc.ifc_buf);
}


int send_arp_req(int sock, in_addr_t ip) {
	unsigned char buf[2*KB] = {0}, broadcast[6] = "\xFF\xFF\xFF\xFF\xFF\xFF";
	struct sockaddr_ll str;
	int r, index, roz_arpha, roz_etha;
	struct ethhdr etha;
	struct arphdr arpha;
	unsigned char ar_sha[ETH_ALEN];	// sender hardware address
	unsigned char ar_tha[ETH_ALEN];	// target hardware address

	/* find interface to send arp from */ 
	for (index = 0; index < descs_count; index++)
		if (descs[index].network == (ip & descs[index].netmask))
			break;
	if (index == descs_count)
		return NW_UNREACH;

	/* fill up the structure for sendto() */
	memset(&str, 0, sizeof(str));
	str.sll_family = PF_PACKET;
	memcpy(str.sll_addr, descs[index].mac, 6);    
	str.sll_halen = 6;
	str.sll_ifindex = descs[index].index;

	//......... creating Ethernet layer ...............

	memcpy(etha.h_dest, broadcast, 6);	// destination eth addr
	memcpy(etha.h_source, descs[index].mac, 6);	// source ether addr
	etha.h_proto = htons(ETH_P_ARP);

	//......... creating ARP layer ...............
	arpha.ar_hrd = htons(ARPHRD_ETHER);		// format of hardware address
	arpha.ar_pro = htons(0x0800); 	// format of protocol address
	arpha.ar_hln = 6;		// length of hardware address
	arpha.ar_pln = 4;		// length of protocol address
	arpha.ar_op = htons(ARPOP_REQUEST);

	memcpy(ar_sha, descs[index].mac, 6);	// sender hardware address
	memset(ar_tha, 0, 6);	// target hardware address

	// ...... creating paket ...............
	roz_arpha = sizeof(arpha);
	roz_etha = sizeof(etha);
	memcpy(buf, &etha, roz_etha);
	memcpy(buf + roz_etha, &arpha, roz_arpha);
	memcpy(buf + roz_etha + roz_arpha, ar_sha, 6);
	memcpy(buf + roz_etha + roz_arpha + 6 + 4, ar_tha, 6);
	memcpy(buf + roz_etha + roz_arpha + 6, &(descs[index].ip), 4);// sender IP address
  
	memcpy(buf + roz_etha + roz_arpha + 6 + 4 + 6, &ip, 4);

	if ((r = sendto(sock, buf, 42, 0, (struct sockaddr*)&str, sizeof(str))) == -1) {
		fprintf(stderr, _("send_arp_reqs: sendto: %s\n"), strerror(errno));
		return NW_UNREACH;
		}

	return ECHO_OK;
}


int recv_arp_reply(host_data *host_info) {
  int buflen = 1*KB;
	unsigned char buf[buflen];
	struct sockaddr_ll str;
	unsigned int len, r, index, roz_arpha, roz_etha;
	unsigned int dstip, srcip;
	struct ethhdr etha;
	struct arphdr *arpha;
	struct timeval stop, actual_time;
	double ping_delay;

	memcpy(&stop, &saved_tv, sizeof(struct timeval));
	memset(buf, 0, buflen);

	roz_arpha = sizeof(*arpha);
	roz_etha = sizeof(etha);

	str.sll_family = PF_PACKET;
	str.sll_protocol = htons(ETH_P_ARP);
	str.sll_hatype = ARPHRD_ETHER;
	str.sll_pkttype = PACKET_HOST;

  len = sizeof(struct sockaddr_ll);
  if ((r = recvfrom(host_info->rawfd, buf, buflen, 0, (struct sockaddr*) &str, &len)) == -1) {
    fprintf(stderr, _("recv_arp_reply: recvfrom: %s, host: %s, errno: %d, buflen %d\n"), strerror(errno), host_info->name, errno, buflen);
    return 1;
  }
  //fprintf(stderr, "arp response is %d bytes\n", r);

  arpha = (struct arphdr *)(buf + sizeof(struct ethhdr));
  //log_event(NULL, std_log, "ar op: %d", ntohs(arpha->ar_op));
  /*
  memcpy(&dstip, buf + roz_etha + roz_arpha + 6 + 4 + 6, 4);
  memcpy(&srcip, buf + roz_etha + roz_arpha + 6, 4);
  log_event(NULL, std_log, "arp dst: %d, src: %d", dstip, srcip);
  sprintf(dstipstr, "%3d.%3d.%3d.%3d", dstip & 255, (dstip >> 8) & 255, (dstip >> 16) & 255, (dstip >> 24) & 255);
  sprintf(srcipstr, "%3d.%3d.%3d.%3d", srcip & 255, (srcip >> 8) & 255, (srcip >> 16) & 255, (srcip >> 24) & 255);
  log_event(NULL, std_log, "arp dst ip: %s, src ip: %s", dstipstr, srcipstr);
  */
  if (ntohs(arpha->ar_op) == ARPOP_REPLY) {
    memcpy(&dstip, buf + roz_etha + roz_arpha + 6 + 4 + 6, 4);
    memcpy(&srcip, buf + roz_etha + roz_arpha + 6, 4);
    /* find target interface for ARP reply */
    for (index = 0; index < descs_count; index++)
      if (descs[index].network == (dstip & descs[index].netmask))
        break;
    if (index >= descs_count) return 0;
    if (srcip == host_info->addr.s_addr) {
      tvsub(&stop, &(host_info->start));
      ping_delay = (double)(stop.tv_sec) * 1000.0 +
                (double)(stop.tv_usec) / 1000.0;
      if (ping_delay > host_info->long_delay)
        set_host_status(host_info, DELAYED);
      else
        set_host_status(host_info, ECHO_OK);
      host_info->delay = ping_delay;
      host_info->lastok_tv = host_info->start;
      host_info->nr_recv++;
      get_actual_time(&actual_time);
      ms_to_tv(tv_to_ms(&actual_time) - host_info->noreply_delay + host_info->ok_delay,
          &(host_info->start));
      host_info->sleep = 1;
    }
  }
	return 0;
}
