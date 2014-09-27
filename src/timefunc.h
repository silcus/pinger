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

#include <sys/time.h>
#include <time.h>

/* Get actual time to timeval struct */
void get_actual_time(struct timeval *act_time);

/* Convert miliseconds to timeval structure */
void ms_to_tv(int ms, struct timeval *tv);

/* Convert timeval structure to miliseconds */
int tv_to_ms(const struct timeval *tv);

/* Timeval struct saved - this can be used for any purposes */
extern struct timeval saved_tv;

/* Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in. */
void tvsub(struct timeval *out, struct timeval *in);

/* get current time str (HH:MM:SS) into time_str. Time can be specified in
 * *time. If time is NULL, actual time is used */
char *get_currtime_str(struct timeval *logtime, char *time_str);

/* get current date and time str (YYYY:MM:DD HH:MM:SS) into timedate_str Time
 * can be specified in *time. If time is NULL, actual time is used*/
char *get_currdatetime_str(struct timeval *logtime, char *datetime_str);
