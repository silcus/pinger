/*
    pinger -- GTK+/ncurses multi-ping utility
    Copyright (C) 2002-2014 Petr Koloros

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

#include <netdb.h>
#include "globals.h"
#include <pthread.h>
#include <signal.h>
#include "dns.h"
#include "log.h"
#include "parse.h"

#ifndef PING_H
#include "ping.h"
#endif



/* semaphor, if resolving, no other resolving can be made at the same time */
int dns_resolving = 0;
/* let's start at the begining */
int resolve_request = 1;

extern pthread_mutex_t cl_mutex;

int resolve_domain_names(hosts_data * hosts)
{
    struct hostent *hent;
    int hcnt;
    host_data *host_info, *host_ptr;

    host_info = hosts->host_info;

    if (dns_resolving == 1)
        return 0;
    dns_resolving = 1;
    resolve_request = 0;
    hcnt = 0;
    while (host_info != NULL) {
        if (strlen(host_info->domain_name)) {
            hent = gethostbyname(host_info->domain_name);
            if ((hent != NULL) && (hent->h_addrtype == AF_INET)) {
                pthread_mutex_lock(&cl_mutex);
                /* Is resolved IP already on the list? */
                host_ptr = hosts->host_info;
                while (host_ptr != NULL) {
                    if ((memcmp
                         (&(host_ptr->addr), (hent->h_addr),
                          sizeof(struct in_addr)) == 0)
                        && (host_ptr->like == NULL) && (host_ptr != host_info))
                        break;
                    host_ptr = host_ptr->next;
                }
                if (host_ptr != NULL)
                    /* then use it's results instead */
                    host_info->like = host_ptr;
                else {
                    /* otherwise set resolved IP and disable substitute (like) */
                    memcpy(&(host_info->addr), hent->h_addr,
                           sizeof(struct in_addr));
                    host_info->like = NULL;
                }
                if (host_info->status == UNRESOLVED)
                    set_host_status(host_info, NO_REPLY);
                pthread_mutex_unlock(&cl_mutex);
            }
        }
        host_info = host_info->next;
    }
    dns_resolving = 0;
    return 0;
}

void *resolve_loop(void *data)
{
    hosts_data *hosts;
    int secs_to_check, sl_time;

    hosts = (hosts_data *) data;
    secs_to_check = dns_check_s;
    if (secs_to_check == -1) {
        resolve_domain_names(hosts);
        return 0;
    }
    while (1) {
        secs_to_check = dns_check_s;
        while ((!resolve_request) && (secs_to_check > 0)) {
            if (DNS_REQ_CHECK_S > secs_to_check)
                sl_time = secs_to_check;
            else
                sl_time = DNS_REQ_CHECK_S;
            /* sleeping is not as much preccious but it doesn't matter here, does it?
             * I'll take another cup of tea and maybe if I have a dream at night
             * about better method, I'll implement it here. */
            sleep(sl_time);
            secs_to_check -= sl_time;
            if (secs_to_check < 0)
                secs_to_check = 0;
        }
        resolve_domain_names(hosts);
    }
    return 0;
}
