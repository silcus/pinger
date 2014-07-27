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

#ifndef DEFINED_GLOBAL
#define DEFINED_GLOBAL 1

#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "../config.h"
#include "fake_gtk.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#define _(Text) gettext(Text)
#define gettext_noop(Text) Text
#define N_(Text) gettext_noop(Text)
#else
#define _(Text) (Text)
#define N_(Text) Text
#undef bindtextdomain
#define bindtextdomain(Domain, Directory) /* empty */
#undef textdomain(Domain) /* empty */
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* define quiet print - print only in non quiet mode */
#define qprint if (!quiet) printf

/* LOCAL makes all variables with EXTERN prefix local.
   If LOCAL not defined those variables will become extern */
#ifdef LOCAL
#define EXTERN
#else
#define EXTERN extern
#endif

#ifndef __USE_MISC
#define unsigned int uint;
#endif

#define MAX_HOSTS 100
/* Maximum size of domain name */
#define MAX_DNAME_LEN 60
/* Maximum size of name */
#define MAX_NAME_LEN 60
/* Special IP representing unresolved host */
#define UNRESOLVED_HOST "0.0.0.0"

/* select timeout for sockets (ms) */
#define SOCK_TIMEOUT 1000

/* maximum of configration files */
#define MAX_CFG_FILES 100

/* info about host */
typedef struct host_data_t {
  int id;          /* nr of host, req. for indexing host list */
  int id_in_set;      /* id inside the set. Good for displaying order, etc. */
  char name[MAX_NAME_LEN + 1];      /* ip description */
  char domain_name[MAX_DNAME_LEN + 1];  /* domain name (being resolved before pinging) */
  struct in_addr addr;  /* ip address */
  int rawfd;        /* socket fd */
  struct timeval start;  /* time of sending last echo */
  struct timeval lastok_tv;   /* timestamp of last ok echo */
  int status;        /* status of host, online, without reply, ... */
  double delay;             /* ping latency in miliseconds */
  u_int last_seq_sent;    /* sequence nr. of last packet sent */
  u_int last_seq_recv;    /* sequence nr. of last packet received */
  unsigned long long nr_sent;        /* number of send and received */
  unsigned long long nr_recv;
  int ok_delay;      /* time in ms, after we send next echo when OK (sucessfuly ping - ECHO REPLY) */
  int noreply_delay;    /* time in ms, after we send next echo when NO_REPLY */
  int long_delay;    /* time in ms, after which host is marked as with long delay */
  int set_nr;       /* number of set in which host is */
  int arp;          /* indicate use of ARP query instead of ICMP ECHO */
  struct host_data_t *next;
  struct host_data_t *like; /* when resolved same IP like other host has, this points
                               to the other host. This host is not used for pinging while
                               results from other host are used. */
  struct timeval last_status_change; /* holds time of last host status change */
  int log_if_status_last;   /* log host change if change last at least this long (seconds) */
  int prev_logged_status;          /* previous logged status */
  int sleep;                /* host can do pinging or sleeping and waiting for next ping cycle */
} host_data;

/* filelist struct to hold multiple configuration files if occur */
typedef struct titlestruct {
  char *title;  /* title of set of hosts */
  int nr;       /* number of ... */
  int refresh_int; /* GUI refresh interval in miliseconds */
  struct titlestruct *next;
} titlelist;

/* info about all hosts; this is useful for carrying all hosts in one pointer
 * (of this structure :)) */
typedef struct {
  int host_max;
  titlelist *titles;
  host_data *host_info;
  int icmp_data_len;
  int icmp_pkt_len;
  unsigned char *icmp_pkt;
} hosts_data;

enum mode_enum {
  NCURSES,
  GTK
};

/* filelist struct to hold multiple configuration files if occur */
typedef struct flstruct {
  char *filename;
  struct flstruct *next;
} filelist;

/* options, which can be configured from configuration file and their default vaules */

/* limits for delays (noreply, ok, longdelay) in miliseconds */
#define MAX_DELAY 3600000
#define MIN_DELAY 0

/* how often display refreshed results on the screen */
#define DEFAULT_REFRESH_INTERVAL 1000
/* after how many second with no answer from host do host timeout */
#define DEFAULT_HOST_TIMEOUT 3000
/* pause between two successfuly pings */
#define DEFAULT_DELAY_BW_PINGS 1000
/* delay in ms after which is delay marked as long */
#define DEFAULT_LONG_DELAY 2000

#ifdef LOCAL
/* hosts specified only by domain names are resolved every 10 minutes. Specify
 * other interval in seconds here if required. -1=disable. */
uint dns_check_s = 600;
#else
extern uint dns_check_s;
#endif

EXTERN int mode;
EXTERN unsigned int quiet;
#endif

char * get_ip_str(struct in_addr addr);
void set_virt_host(host_data *host, host_data *virt_host);
