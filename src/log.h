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

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define LOG_STATUS_LAST -1 /* log host status change if it lasts at least this long in seconds.
                              0 means log every change and ingnore duration of the status.
                              -1 = disable host status logging. */
#define MIN_STATUS_LAST -1
#define MAX_STATUS_LAST 84000
#define DEFAULT_STATUS_LAST 0

typedef struct log_struct{
  FILE *file;
  char *log_name;
  int host_set;
  struct log_struct *next;
} log_struct_t;

typedef struct logs_struct {
  log_struct_t *log;
  int type;
} logs_t;

enum log_types {
  ERR_LOG,
  HOST_LOG
};

#ifndef LOG_LOCAL
extern logs_t *err_log;
extern logs_t *host_log;
#endif

/* Error status codes when initializing log. LOG_OK is first one with code 0 */
enum log_errors {
  LOG_OK,
  LOG_BAD_FILE,
  LOG_SET_EXISTS
};

#define LOG_DEFAULT -2
#define LOG_DISABLED -1
#define LOG_ENABLED 0

/* Init log inside logs. If logs struct is not initalized, do initialize it. */
int init_log(logs_t *logs, char *log_file_name, char *log_name, int host_set);

/* Return meaning of the log error as string */
char *log_error_str(int error);

/* Close specified logs */
int close_logs(logs_t *logs);

/* Close specified log from logs */
int close_log(logs_t *logs, log_struct_t *log);

/* Close all logs, which are defined inside log.c file */
void close_built_in_logs();

/* Init all logs which are defined inside log.c file*/
void init_built_in_logs();

/* Log event into specified log file. Some are defined inside parse.c source as
 * global. */
int log_event(struct timeval *logtime, logs_t *logs, int host_set, char *event, ...);

