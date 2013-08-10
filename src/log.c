/*
    pinger -- GTK+/ncurses multi-ping utility
    Copyright (C) 2006 Petr Koloros

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

#include "timefunc.h"
#include <stdarg.h>

#define LOG_LOCAL 1
#include "log.h"
/* These are log roots. Every log begins here. */
logs_t err_logs;
logs_t host_logs;
logs_t *err_log = &err_logs;
logs_t *host_log = &host_logs;
#include "parse.h"


/* Init all logs which are defined inside log.c file */
void init_built_in_logs()
{
  err_logs.log = NULL;
  host_logs.log = NULL;
  err_logs.type = ERR_LOG;
  host_logs.type = HOST_LOG;
}

/* Return meaning of the log error as string */
char *log_error_str(int error)
{
  switch (error) {
    case LOG_BAD_FILE:
      return _("File does not exist and cannot be created.");
      break;
    case LOG_SET_EXISTS:
      return _("This host set has already initialized log.");
      break;
  }

  return _("Unknown error.");
}

/* Init log inside logs. If logs struct is not initalized, do initialize it. */
int init_log(logs_t *logs, char *log_file_name, char *log_name, int host_set)
{
  log_struct_t * log_ptr;
  FILE *f;

  if ((log_file_name != NULL) || (logs == NULL)) {
    qprint("%s: %s\n", _("Initializing log"), log_name);
    f = fopen(log_file_name, "a+");
    if (f == NULL) {
      log_event(NULL, err_log, -1, "%s: %s\n", log_name, _("Cannot open log file"), strerror(errno)); 
      return LOG_BAD_FILE;
    } else {
      qprint("%s: %s\n", log_name, _("File opened"));
      log_ptr = logs->log;
      if (log_ptr == NULL) {
        logs->log = malloc(sizeof(log_struct_t));
        log_ptr = logs->log;
      } else {
        while ((log_ptr->next != NULL) && (log_ptr->host_set != host_set)) log_ptr = log_ptr->next;
        if (log_ptr->host_set == host_set) return LOG_SET_EXISTS;
        log_ptr->next = malloc(sizeof(log_struct_t));
        log_ptr = log_ptr->next;
      }
      log_ptr->host_set = host_set;
      log_ptr->next = NULL;
      log_ptr->log_name = malloc(strlen(log_name)+1);
      strcpy(log_ptr->log_name, log_name);
      log_ptr->file = f;
      log_event(NULL, logs, host_set, "%s %s %s", _("Pinger version"), VERSION, _("started."));
    }
  }
  return LOG_OK;
}

/* Close specified log from logs */
int close_log(logs_t *logs, log_struct_t *log)
{
  log_struct_t *log_ptr, *prev_ptr;

  if ((log != NULL) && (logs->log != NULL)) {
    log_ptr = logs->log;
    prev_ptr = log_ptr;
    while ((log_ptr != NULL) && (log_ptr != log)) {
      prev_ptr = log_ptr;
      log_ptr = log_ptr->next;
    }
    /* quit if not found */
    if (log_ptr == NULL) 
      return -1;
    log_event(NULL, logs, log_ptr->host_set, "%s %s %s", _("Pinger version"), VERSION, _("stopped"));
    /* if first in the struct, set that pointer in the struct to the next one */
    if (log_ptr == logs->log) logs->log = log_ptr->next;
    else prev_ptr->next = log_ptr->next;
    free(log_ptr->log_name);
    fclose(log_ptr->file);
    free(log_ptr);
  }

  return 0;
}

/* Close all logs */
int close_logs(logs_t *logs)
{
  log_struct_t *log_next, *log;
  int ret = 0;

  log = logs->log;
  while (log != NULL) {
    log_next = log->next;
    if (close_log(logs, log) == -1)
      ret = -1;
    log = log_next;
  }

  return ret;
}

/* Close all logs, which are defined in this file */
void close_built_in_logs()
{
  close_logs(err_log);
  close_logs(host_log);
}


/* Log event to the specified file (given inside logs structure, while each host_set could have it's
 * own log file). If host_set is -1, stderr is used instead */
int log_event(struct timeval *logtime, logs_t *logs, int host_set, char *event, ...)
{
  char datestr[30];
  va_list params;
  char buf[1024];
  FILE *f;
  log_struct_t *log_ptr;

  if (host_set == -1) f = stderr;
  else {
    if ((logs == NULL) || (logs->log == NULL)) return 0;
    log_ptr = logs->log;
    while ((log_ptr != NULL) && (log_ptr->host_set != host_set)) log_ptr = log_ptr->next;
    if (log_ptr == NULL) return 0;
    f = log_ptr->file;
  }
  va_start(params, event);
  vsnprintf(buf, sizeof(buf), event, params);
  va_end(params);
  
  if (f != NULL) {
    get_currdatetime_str(logtime, datestr);
    if (f == stderr) 
      fprintf(f, "%s: %s\n", _("Error"), buf);
    else 
      fprintf(f, "%s: %s\n", datestr, buf);
    fflush(f);
  }

  return 0;
}


