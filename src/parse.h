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


#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "globals.h"

#ifdef ENABLE_ARP
#ifndef ETHER_H
#define ETHER_H
#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>       /* the L2 protocols */
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>     /* The L2 protocols */
#endif
#endif
#endif

/* how many characters (plus term. char.) can byte have (in string form) */
#define BYTE_STR_LEN 4
#define CFG_FILENAME ".pingerrc"    /* may be overriden by cfg_filename variable */
#define MAX_CFGLINE_LEN 100     /* this is maximum length of each cfg line which will be used */
#define LONG_CFGLINE_LEN 500    /* here will be each line read first to decide what length it actually has */

/* maximum value of any option */
#define MAX_OPT_VALUE 65535
/* default name of the host set. ! Avoid longer title than allowed size below ! */
#define STD_TITLE "standard set"
/* maximum allowed title length */
#define MAX_TITLE_LEN 20
#define MIN_TITLE_LEN 2         /* at least two digits */
#define MAX_TABS 20


/* Parse one line of config file ("name = value" format)
 * and store values into "name" and "value" */
int parse_line(char *line, char *name, char *value);

/* Check if byte (in string form) is real byte value (0-255) */
int check_byte(char *byte);

/* Check if valid option value */
int valid_opt_value(char *value);

/* Check if IP ("XXX.XXX.XXX.XXX" in string form) is real IPv4 IP address */
int valid_ip(char *ip);

/* Read config file and fill up host structure with these data.
 * Also open raw sockets for the hosts. */
void read_config_file(hosts_data * hosts, filelist * cfg_filenames);
