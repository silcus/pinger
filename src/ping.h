/*
    pinger -- GTK+/ncurses multi-ping utility
    Copyright (C) 2002 Petr Koloros

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef PING_H
#define PING_H
#include "fake_gtk.h"

/* Default icmp data length */
#define DEFAULT_ICMP_DATA_LEN 0
#define MAX_ICMP_DATA_LEN 65507 /* 65535-20-8 = max ip data len - ip hdr - icmp hdr */
#define MIN_ICMP_DATA_LEN 0

/* send ICMP/arp "echo" to specified host.
 * Return OK or NW_UNREACH */
int send_echo(host_data * host, hosts_data * hosts);

/* send ICMP echo_request to specified host.
 * Return OK or NW_UNREACH */
int send_icmp_echo(host_data * host, hosts_data * hosts);

/* send ARP request to specified host */
int send_arp(host_data * host);

/* send initial ICMP/ARP echo to all host */
void pinger_init(host_data * host_info, hosts_data * hosts);

/* process icmp/arp reply */
void process_reply(host_data * host_info);

/* process icmp reply - respond to the answer and send another ICMP echo if necessary */
void process_icmp_reply(host_data * host_info);

/* Checking for offline timeouts and provides interval between ICMP echo sending */
void timeout_check(hosts_data * hosts);

/* allocate icmp packet. data_len is the payload size of
 * the icmp packet. Return value is the total length of
 * the packet. */
int allocate_icmp_packet(unsigned char **packet, int data_len);

/* Free allocated raw sockets */
int free_sockets(hosts_data * hosts);

/* define states for host_data.status */
enum status_enum
{
    ECHO_OK,
    NO_REPLY,
    NW_UNREACH,
    DELAYED,
    NO_VALID_ANS,
    UNRESOLVED
};

/* return host status as string */
char *host_status_str(int status);

/* set host status and log change if requred */
void set_host_status(host_data * host, int status);


#endif
