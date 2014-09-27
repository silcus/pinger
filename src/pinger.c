/*
    pinger -- GTK+/ncurses multi-ping utility
    Copyright (C) 2002-2006 Petr Koloros

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


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>

/* this defines variables from globals.h as local to this file */
#define LOCAL
#include "globals.h"
#include "ping.h"
#include "timefunc.h"
#include "parse.h"
#include "dns.h"
#include "log.h"

#ifdef ENABLE_ARP
#include "arp.h"
#endif

#ifdef HAVE_LIBNCURSES
#include "interface_ncurses.h"
#endif
#if defined(HAVE_GTK_2) || defined(HAVE_GTK_3)
#include "interface_gtk.h"
#endif

#define AUTHOR_STR "Petr Koloros, silk[a t]seznam[d o t]cz"

//#define DEBUG

/* guiloop checks this periodicaly */
int stop_gui_loop = 0;

pthread_mutex_t cl_mutex = PTHREAD_MUTEX_INITIALIZER;

struct child_data_struct {
  host_data *host_info;
  int *parent_living;
  hosts_data *hosts;
};

typedef struct child_data_struct child_data_t;

/* show_status_function - like prototype, independent of output interface (gtk, ncurses, ..) */
int (*show_status)(host_data * host);

void print_startup_info() {
  qprint("\n%s %s, %s\n\n", _("Pinger version"), VERSION, AUTHOR_STR);
}

void child_down_handler(int signum)
{
  stop_gui_loop = 1;
}


/* ------- CHILD --------- */
/* child begins here, let it be engine doing all the pinging stuff */
void *child_loop(void *data)
{
  fd_set fdset;
  struct timeval cur_timeout;
  int selected;
  child_data_t *child_data;
  int *parent_living;
  host_data *host_info;
  hosts_data *hosts;
  int fd_max;

  child_data = (child_data_t *)data;
  host_info = child_data->host_info;
  parent_living = child_data->parent_living;
  hosts = child_data->hosts;

  FD_ZERO(&fdset);
  /* send initial ICMP echo to all hosts */
  pinger_init(host_info, hosts);

  fd_max = 0;
  while (host_info != NULL) {
    if (host_info->rawfd > fd_max) fd_max = host_info->rawfd;
    host_info = host_info->next;
  }
  host_info = child_data->host_info;

  /* engine loop */
  while ((*parent_living == 1) && (stop_gui_loop == 0)) {
    /* refresh interval for display (default or defined in config file) */
    ms_to_tv(SOCK_TIMEOUT, &cur_timeout);

    /* set all FDs */
  
    while (host_info != NULL) {
      if (!FD_ISSET(host_info->rawfd, &fdset))
          FD_SET(host_info->rawfd, &fdset);
      host_info = host_info->next;
    }
    host_info = child_data->host_info;
      
    /* select, fd_max is highest used fd given acquired when parsing cfg file */
    if (((selected = select(fd_max+1, &fdset, NULL, NULL, &cur_timeout)) < 0 ) &&
        (errno != EINTR)) {
      log_event(NULL, err_log, -1, "%s %s", _("Cannot select:"), strerror(errno));
      exit(1);
    }
    
    /* remember reply time */
    get_actual_time(&saved_tv);
    
    if ((*parent_living == 0) || (stop_gui_loop == 1)) break;
    pthread_mutex_lock(&cl_mutex);
    if (selected > 0) {
      while (host_info != NULL) {
        if ((FD_ISSET(host_info->rawfd, &fdset)) /*&& (selected)*/) 
          process_reply(host_info);
        host_info = host_info->next;
      }
      host_info = child_data->host_info;
    }
    if (stop_gui_loop == 0) timeout_check(hosts);
    pthread_mutex_unlock(&cl_mutex);
  }

  exit(0);
}

/* Return IP as a string */
char * get_ip_str(struct in_addr addr) {
  if (strcmp(inet_ntoa(addr), UNRESOLVED_HOST) == 0) return "---";
  else return inet_ntoa(addr);
}

/* Set virtual host. When domain name is resolved to the ip
 * which already exists in the host list, this ip's results
 * are used instead and hence we require virtual host which
 * represents the original or the fake one which leads to the
 * already-on-the-list one. */
void set_virt_host(host_data *host, host_data *virt_host) {
  int namelen;
  
  if (host->like != NULL) {
    memcpy(virt_host, host->like, sizeof(host_data));
    /* no more recursion */
    virt_host->like = NULL;
    /* id in set must remain original */
    virt_host->id_in_set = host->id_in_set;
    strcpy(virt_host->name, host->name);
    namelen = strlen(host->name);
    if (namelen >= MAX_NAME_LEN)
      virt_host->name[namelen - 1] = '*';
    else {
      virt_host->name[namelen] = '*';
      virt_host->name[namelen + 1] = 0;
    }
  } else memcpy(virt_host, host, sizeof(host_data));
}

/* main of the pinger */
int main( int   argc,
          char *argv[] )
{
  /* pinger variables */
  hosts_data hosts;
  int a;
  int parent_living;
  filelist *flist = NULL, *flistidx = NULL;
  int flist_cnt = 0;
  void (*interface_init)(int *argc, char **argv[], \
      char *title, hosts_data *hosts);
  void (*interface_done)(char *error_str);
  void (*gui_loop)(hosts_data *hosts, int *stop_loop);
  pthread_attr_t pth_attr;
  pthread_t thread, dns_thread;
  child_data_t child_data;
  uid_t saved_euid, saved_uid;

  /* save root privileges unless neccessary */
  saved_euid = geteuid();
  if (saved_euid != 0) {
    printf("%s\n", _("Not running with root privileges, make sure this application has SUID bit set."));
    exit(1);
  }
  if (seteuid(getuid()) != 0) {
    printf("%s\n", _("Cannot restore back user's privileges, quitting."));
    exit(1);
  }
    
  signal(SIGCHLD, child_down_handler);
  signal(SIGPIPE, child_down_handler);

#ifdef ENABLE_NLS
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  init_built_in_logs();

/* trying to set default mode as defines enable */
#ifndef HAVE_LIBNCURSES
#if defined(HAVE_GTK_2) || defined(HAVE_GTK_3)
  mode = GTK;
#else
#error There must be some defined GUI mode
#endif
#else
  mode = NCURSES;
#endif
 
  /* sanity checks */
  if (MAX_CFGLINE_LEN < MAX_TITLE_LEN) {
    log_event(NULL, err_log, -1, "%s %s!! %s", "MAX_TITLE_LEN", _("must be less than"), "MAX_CFGLINE_LEN", ("Someone doesn't know how to make changes in the source code!"));
    exit(1);
  }
  if (strlen(STD_TITLE) > MAX_TITLE_LEN) {
    log_event(NULL, err_log, -1, "%s %s!! %s", _("must be greater than"), "MAX_TITLE_LEN", _("Someone doesn't know how to make changes in the source code!"));
    exit(1);
  }
  if (MIN_TITLE_LEN > MAX_TITLE_LEN) {
    log_event(NULL, err_log, -1, "%s %s!! %s", "MIN_TITLE_LEN", _("must be greater than"), "MAX_TITLE_LEN", _("Someone doesn't know how to make changes in the source code!"));
    exit(1);
  }
  
  /* verbose output by default */
  quiet = 0;

  /* default values for parameters which mat get changed in following code */
  hosts.icmp_data_len = DEFAULT_ICMP_DATA_LEN;
  hosts.icmp_pkt_len = 0;
  hosts.icmp_pkt = NULL;

  /* Check for parameters */
  if (argc > 1) {
    for (a = 1; a < argc; a++) {
      if (!strcmp(argv[a], "--gtk")) {
        mode = GTK;
      } else if (!strcmp(argv[a], "--help")) {
        printf("\n%s %s - Copyright 2002-2005 %s, GPL'ed\n", _("Pinger - version"), VERSION, AUTHOR_STR);
        printf("\n%s\n", _("Usage:"));
        printf("    pinger [--gtk] [--quiet] [conffile1] [conffile2] ..\n\n");
        printf("%s\n", _("Parameters:"));
        printf("    --gtk   : %s\n", _("use GTK 2.x frontend (otherwise use ncurses)."));
        printf("    --quiet : %s\n\n", _("do not display messages into console."));
        exit(EXIT_FAILURE);
      } else if (!strcmp(argv[a], "--quiet"))
        quiet = 1;
      /* handle arg as file */
      else if (++flist_cnt < MAX_CFG_FILES) {
        if (flistidx == NULL) {        
          flistidx = malloc(sizeof(filelist));
          flist = flistidx;
        } else {
          flistidx->next = malloc(sizeof(filelist));
          flistidx = flistidx->next;
        }
        flistidx->filename = malloc(strlen(argv[a]) + 1);
        strcpy(flistidx->filename, argv[a]);
        flistidx->next = NULL;
      } else if (flist_cnt > MAX_CFG_FILES) 
        log_event(NULL, err_log, -1, 
            _("Maximum configuration files count exceeded. All following files will be ignored"));
      /* end handle file */
          
    }
  }

#ifdef HAVE_LOCALE_H
  setlocale(LC_ALL, "");
#endif

  qprint(_("Available modes:"));
#ifdef HAVE_LIBNCURSES
  qprint(" ncurses");
#endif
#ifdef HAVE_GTK_2
  qprint(" GTK (v2)");
#endif
#ifdef HAVE_GTK_3
  qprint(" GTK (v3)");
#endif
  qprint("\n%s ", _("Using mode:"));
  
  if (mode == GTK) {      
#if !defined(HAVE_GTK_2) && !defined(HAVE_GTK_3)
    log_event(NULL, err_log, -1, _("GTK mode is not enabled, \
please recompile this program with enabling GTK mode."));
    exit(EXIT_FAILURE);
#endif
    qprint("GTK\n");
#if defined(HAVE_GTK_2) || defined(HAVE_GTK_3)
#if defined(DEBUG)
    log_event(NULL, err_log, -1, "setting gtk2 interface functions\n");
#endif
    show_status = gtk_show_status;
    interface_init = gtk_interface_init;
    gui_loop = gtk_gui_loop;
    interface_done = gtk_interface_done;
#endif
  } 

  if (mode == NCURSES) {
#ifndef HAVE_LIBNCURSES
    log_event(NULL, err_log, -1, _("ncurses mode is not enabled, \
please recompile this program with enabling ncurses mode."));
    exit(EXIT_FAILURE);
#endif
    qprint("ncurses\n");
#ifdef HAVE_LIBNCURSES
#if defined(DEBUG)
    log_event(NULL, err_log, -1, "setting ncurses interface functions\n");
#endif
    show_status = ncurses_show_status;
    interface_init = ncurses_interface_init;
    gui_loop = ncurses_gui_loop;
    interface_done = ncurses_interface_done;
#endif
  }
  
  /* this says that parent is living. If not, child will terminate itselfs */
  parent_living = 1;
  
  hosts.host_info = NULL;
  hosts.host_max = 0;
  hosts.titles = NULL;

  if (seteuid(saved_euid) != 0) {
    log_event(NULL, err_log, -1, "%s", _("Cannot restore back root privileges, quiting."));
    exit(1);
  }

  read_config_file(&hosts, flist);

  if (hosts.host_max == 0) {
    log_event(NULL, err_log, -1, _("No hosts loaded!"));
    exit(EXIT_FAILURE);
  }
  
  
#ifdef ENABLE_ARP
  if (seteuid(saved_euid) != 0) {
    log_event(NULL, err_log, -1, "%s %s", _("ARP code"), _("Cannot set root privileges, quitting."));
    exit(1);
  }
  get_ifaces();
  if (seteuid(getuid()) != 0) {
    log_event(NULL, err_log, -1, "%s %s", _("ARP code"), _("Cannot restore back user's privileges, quitting."));
    exit(1);
  }
#endif

  if (seteuid(getuid()) != 0) {
    log_event(NULL, err_log, -1, "%s", _("Cannot restore back user's privileges, quitting."));
    exit(1);
  }

  /* Print info about program to console */
  print_startup_info();

  saved_uid = getuid();
  if (setresuid(saved_uid, saved_uid, saved_uid) == -1) {
    qprint("%s\n", _("Cannot get rid of effective and saved uid."));
  } else qprint("%s\n", _("Successfuly removed root privileges."));

  if ((hosts.icmp_pkt_len = allocate_icmp_packet(&(hosts.icmp_pkt), hosts.icmp_data_len)) == -1) {
    log_event(NULL, err_log, -1, "%s", _("Cannot allocate icmp packet (bad icmp 'packetsize' perhaps?)."));
    exit(1);
  }

  /* Initialization complete, now fork into two processes:
   * parent display: periodically displays informations about host
   * child engine: do pinging
   * Second writes to shared memory and first only reads data from it */

  child_data.hosts = &hosts;
  child_data.host_info = hosts.host_info;
  child_data.parent_living = &parent_living;
  //child_data.saved_euid = saved_euid;

  pthread_attr_init(&pth_attr);
  pthread_attr_setdetachstate(&pth_attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&thread, &pth_attr, &child_loop, (void *)(&child_data));
  pthread_attr_destroy(&pth_attr);

  /* create thread for resolving domain addresses */
  pthread_attr_init(&pth_attr);
  pthread_attr_setdetachstate(&pth_attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&dns_thread, &pth_attr, &resolve_loop, (void *)(&hosts));
  pthread_attr_destroy(&pth_attr);

  /* ------- PARENT --------- */
  /* parent begins here, let it be gui, displaying results periodically */

#if defined(DEBUG)
  qprint("Calling interface init\n");
#endif
  interface_init(&argc, &argv, hosts.titles->title, &hosts);
  
#if defined(DEBUG)
  qprint("Calling gui loop\n");
#endif
  gui_loop(&hosts, &stop_gui_loop);
#if defined(DEBUG)
  qprint("stop_gui_loop=%d\n", stop_gui_loop);
#endif
  
  if (stop_gui_loop)
    interface_done(_("Parent is commiting suicide because its only child died unexpectedly."));
  else
    interface_done(NULL);

  /* tell child to deallocate memory */
  parent_living = 0;
  
  close_built_in_logs();
  return EXIT_SUCCESS;
}

