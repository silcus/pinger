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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "globals.h" /* only for gettext support */

/* Timeval struct saved - this can be used for any purposes */
struct timeval saved_tv;

/* Get actual time to timeval struct */
void get_actual_time (struct timeval * act_time) {
	if (gettimeofday(act_time, NULL) < 0) {
		fprintf(stderr, "%s\n", _("Cannot get time of day."));
		exit(1);
	} 
}

/* Convert miliseconds to timeval structure */
void ms_to_tv (int ms, struct timeval *tv) {
        tv->tv_sec = ms / 1000;
        tv->tv_usec = (ms % 1000) * 1000;
}

/* Convert timeval structure to miliseconds */
int tv_to_ms (const struct timeval *tv) {
        return tv->tv_sec * 1000 + tv->tv_usec / 1000;
}

/* Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in. */
void tvsub (struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/* get current time str (HH:MM:SS) into time_str */
char * get_currtime_str(struct timeval *logtime, char *time_str)
{
  struct tm *tmstruct;
  time_t act_time;

  if (logtime != NULL) act_time = (time_t)(logtime->tv_usec);
  else time(&act_time);
  tmstruct = localtime(&act_time);
  snprintf(time_str, 9, "%02d:%02d:%02d\n", tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
  return time_str;
}
         
/* get current date and time str (YYYY:MM:DD HH:MM:SS) into timedate_str */
char * get_currdatetime_str(struct timeval *logtime, char *datetime_str)
{
  struct tm *tmstruct;
  time_t act_time;

  if (logtime != NULL) act_time = (time_t)(logtime->tv_sec);
  else time(&act_time);
  tmstruct = localtime(&act_time);
  strftime(datetime_str, 21, "%Y-%m-%d %H:%M:%S", tmstruct);
  return datetime_str;
}
  

