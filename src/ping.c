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


#include "globals.h"
#include "ping.h"
#include "log.h"
#include "timefunc.h"
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef ENABLE_ARP
#include <arp.h>
#endif

/* Compute checksum for ICMP output buffer */
static u_short cksum (u_short *buf, u_int len)
{
	u_int sum = 0;
	u_short retval;

	/* 32 bit sum adds sequential 16 bit buffer parts */
	while (len > 1) {
		sum += *buf++;
		len -= 2;
	}
	
	/* if only one byte pending, set the half of word to zero and other half to this byte */
	if (len == 1) {

		union {
			u_short word;
			u_char byte;
		} odd;

		odd.word = 0;
		odd.byte = *(u_char *) buf;
		sum += odd.word;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	retval = ~sum;
	return retval;
}

/* return host status as string */
char *host_status_str(int status)
{
  switch (status) {
    case ECHO_OK:     return _("Online");
                      break;
    case NO_REPLY:    return _("Offline");
                      break;
    case NW_UNREACH:  return _("Unreachable");
                      break;
    case DELAYED:     return _("Delayed");
                      break;
    case NO_VALID_ANS:  return _("No valid answer");
                      break;
    case UNRESOLVED:  return _("Unresolved");
                      break;
  }
  return _("Unknown status");
}

/* set host status and log change if requred */
void set_host_status(host_data *host, int status)
{
  struct timeval act_time, tmp_time;
  
  get_actual_time(&act_time);
  memcpy(&tmp_time, &act_time, sizeof(act_time));
  if ((host->log_if_status_last > 0) && (host->prev_logged_status != status)) {
    tvsub(&tmp_time, &(host->last_status_change));
    if (tmp_time.tv_sec >= host->log_if_status_last) {
      log_event(&(host->last_status_change), host_log, host->set_nr, 
          "%s %s %s %s", _("Host"), host->name, _("changed status to:"), host_status_str(status));
      host->prev_logged_status = status;
    }
  }
  if (status != host->status) {
    if (host->log_if_status_last == 0) {
      log_event(NULL, host_log, host->set_nr, "%s %s %s %s", _("Host"), host->name, _("changed status to:"), host_status_str(status));
    }
    memcpy(&(host->last_status_change), &act_time, sizeof(act_time));
    host->status = status;
  }
}

int allocate_icmp_packet(unsigned char **packet, int data_len)
{
  int pl;

  if (*packet != NULL) return -1;
  if (data_len > MAX_ICMP_DATA_LEN) return -1;
  pl = sizeof(struct icmphdr) + data_len; /* icmp hdr + icmp data */ 
  if ((*packet = (unsigned char *)malloc(pl)) == NULL) return -1;
  return pl;
}


/* send ARP request to specified host */
int send_arp(host_data *host) {
#ifdef ENABLE_ARP
  return send_arp_req(host->rawfd, host->addr.s_addr);
#endif
}

int send_echo(host_data *host, hosts_data *hosts) {
  int retval;

  if (host->like != NULL) return 0;
  get_actual_time(&(host->start));
  host->sleep = 0;
#ifdef ENABLE_ARP
  if (host->arp) {
    retval = send_arp(host);
    if (retval == ECHO_OK) host->nr_sent++;
    return retval;
  } else
#endif
    return send_icmp_echo(host, hosts);
}

/* send ICMP echo_request to specified host */
int send_icmp_echo(host_data *host, hosts_data *hosts) {

	struct sockaddr_in to;
	struct icmphdr *ich;
	int retval;
  unsigned char *outpacket;
  int outpacket_len;
 
  if (hosts->icmp_pkt == NULL) { 
    hosts->icmp_data_len = 0;
    hosts->icmp_pkt_len = sizeof(struct icmphdr);
    hosts->icmp_pkt = (unsigned char *)malloc(hosts->icmp_pkt_len);
  }
  outpacket = hosts->icmp_pkt;
  outpacket_len = hosts->icmp_pkt_len;

	memset(&to, '\0', sizeof(to));
	to.sin_port = 0;
	to.sin_family = AF_INET;
	to.sin_addr = host->addr;
	ich = (struct icmphdr *) outpacket;
	ich->type = ICMP_ECHO;
	ich->code = 0;
	ich->checksum = 0;
	ich->un.echo.sequence = ++host->last_seq_sent;
	ich->un.echo.id = getpid() & 0xFFFF;
	ich->checksum = cksum ((u_short *) outpacket, outpacket_len);

	if ((sendto (host->rawfd, outpacket, outpacket_len, 0, (struct sockaddr *)&to, sizeof(to))) < 0) {
    set_host_status(host, NW_UNREACH);
		retval = NW_UNREACH;
	} else {
    host->nr_sent++;
    retval = ECHO_OK;
  }
	return retval;
}

/* send initial ICMP/ARP echo to all host */
void pinger_init(host_data * host_info, hosts_data *hosts) {
	
  host_data *host = host_info;
  
  while (host != NULL) {
    host->status = NO_REPLY;    /* initial status = offline */
    host->prev_logged_status = NO_REPLY;
    if (strcmp((char *)inet_ntoa(host->addr), UNRESOLVED_HOST) == 0) {
      set_host_status(host, UNRESOLVED);
    } else if (send_echo(host, hosts)) {
      set_host_status(host, NW_UNREACH);
		}
		get_actual_time(&(host->start));
		get_actual_time(&(host->last_status_change));
    host = host->next;
	}
}


/* process icmp/arp reply */
void process_reply(host_data *host_info) {
  
#ifdef ENABLE_ARP
  if (host_info->arp) recv_arp_reply(host_info);
  else 
#endif
    process_icmp_reply(host_info);
}

/* process icmp reply - respond to the answer and send another ICMP echo if necessary */
void process_icmp_reply(host_data *host_info) {
	
	struct sockaddr_in from;
	socklen_t fromlen = sizeof (from);
	struct iphdr *iph;
	struct icmphdr *ich;
	unsigned char inpacket[IP_MAXPACKET];
	int nrecv;
	double ping_delay;
	struct timeval stop, actual_time;

  if (host_info->arp) {
#ifdef ENABLE_ARP
    printf("Arp is enabled");
#endif
    log_event(NULL, err_log, host_info->set_nr, "%s %s.", 
        _("This packet is ARP and processed via process_icmp. It's definitely wrong. Host"), host_info->name);
    exit(1);
  }
  memcpy(&stop, &saved_tv, sizeof(struct timeval));
	
	/* receive packet */
	nrecv = recvfrom (host_info->rawfd, inpacket, sizeof (inpacket), 0, (struct sockaddr *)&from, &fromlen);
	if (nrecv < 0) {
		log_event(NULL, err_log, host_info->set_nr, "process_icmp_reply recvfrom: %s", strerror (errno));
		exit(1);
	}

	/* test for packet validity */
	if ((size_t) nrecv < sizeof (struct iphdr)) {
		log_event(NULL, err_log, host_info->set_nr, "process_icmp_reply: %s", _("Packet too short!"));
	}

	iph = (struct iphdr *) inpacket;

	if (iph->version != 4) {
		log_event(NULL, err_log, host_info->set_nr,  "process_icmp_reply: %s", _("Wrong IP version!"));
		exit(1);
	}
	if (iph->protocol != IPPROTO_ICMP) {
		log_event(NULL, err_log, host_info->set_nr, "process_icmp_reply: %s", _("Packet is not ICMP!"));
		exit(1);
	}
  if (iph->ihl * 4 + sizeof (struct icmphdr) > (size_t) nrecv) {
		log_event(NULL, err_log, host_info->set_nr, "process_icmp_reply: %s", _("Packet too short!"));
		exit(1);
	}
	ich = (struct icmphdr *) (inpacket + iph->ihl * 4);

	/* print result */
	if (from.sin_addr.s_addr == host_info->addr.s_addr) {
		tvsub(&stop, &(host_info->start));
		ping_delay = (double)(stop.tv_sec) * 1000.0 +
	    				(double)(stop.tv_usec) / 1000.0;
		if (ich->un.echo.id == getpid()) {
			if (ich->type == ICMP_ECHOREPLY) {
				if ((host_info->last_seq_sent = ich->un.echo.sequence)) {
				
          if (ping_delay > host_info->long_delay)
            set_host_status(host_info, DELAYED);
          else
            set_host_status(host_info, ECHO_OK);
					host_info->delay = ping_delay;
          host_info->lastok_tv = host_info->start;
          host_info->nr_recv++;

					/* show_status(host_info); */

					/* Now the ping was OK and we want to wait delay_between_pings_ms ms.
					 * We will use offline mechanism wrote bellow to acquire the right time
					 * for sending next ping to this host after the delay.
					 * For Example: host_timeout = 3s, delay = 500ms, so we want to set
					 * host 'start' timer to (500ms - 3s) = -2,5 seconds. This causes the
					 * host to timeout after 500ms in 'offline timeout mechanism' which
					 * is set to timeout everything longer than 3 seconds. */
					get_actual_time(&actual_time);
					ms_to_tv(tv_to_ms(&actual_time) - host_info->noreply_delay + host_info->ok_delay, &(host_info->start));
          host_info->sleep = 1;
				}
			} 
		}
	}
}

/* Check all hosts for offline timeouts */
void timeout_check(hosts_data *hosts) {
	
	host_data * host_info;
	struct timeval actual_time;
	
	host_info = hosts->host_info;

	/* offline timeout mechanism. Check every timer if longer than allowed amount
	 * of time. If longer, claim it offline and try send another ping. If status is 
	 * ECHO_OK, it means that host waited to send another ping. Only NO_REPLY packets can
	 * be claimed as offline
	 */
	get_actual_time(&actual_time);
  while (host_info != NULL) {
    if (host_info->like != NULL) {
      host_info = host_info->next;
      continue;
    }
		if ((tv_to_ms(&actual_time) - tv_to_ms(&(host_info->start))) > host_info->noreply_delay) {
      if (strcmp((char *)inet_ntoa(host_info->addr), UNRESOLVED_HOST) != 0) {
        if (host_info->sleep == 0) {
          /* Every resent packet has NO_REPLY status. If there is no answer in 
          * host_timeout_ms ms, it means, that we still got host with NO_REPLY
          * status and it means it.
          */
          set_host_status(host_info, NO_REPLY);
        }
        send_echo(host_info, hosts);
      }
		}
    host_info = host_info->next;
	}
}


/* Free allocated raw sockets */
int free_sockets(hosts_data *hosts)  {

	host_data * host_info;
	int retval;

	retval = 0;
	
	host_info = hosts->host_info;

  while (host_info != NULL) {
    printf("%s %s", _("Releasing socket for"), inet_ntoa(host_info->addr));
    if (strcmp((char *)inet_ntoa(host_info->addr), "0.0.0.0") == 0) printf(" %s\n",
        _("(unresolved host)"));
    else printf("\n");
    if (close(host_info->rawfd)) {
      log_event(NULL, err_log, host_info->set_nr, "%s %s", _("Cannot release raw socket for ip"), inet_ntoa(host_info->addr));
      retval = 1;
    }
    host_info = host_info->next;
	}

	return retval;
}

