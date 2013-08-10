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

/* Hosts specified only with domain names are resolved periodicaly within
 * defined interval (configure file). Inside this interval there are made small
 * checks every DNS_REQ_CHECK_S seconds if there is any request waiting to be
 * processed before regular dns check interval */
#define DNS_REQ_CHECK_S 1
#define DNS_CHECK_MIN_S 1
#define DNS_CHECK_MAX_S 86400

/* By default a-z,A-Z is allowed in domain name. These specifies extra
 * characters which are allowed */
#ifdef DNS_LOCAL
char dns_allowed_chars[2] = {'-', '.'};
#else
extern char *dns_allowed_chars;
#endif

/* This loop resolves domain names periodicaly */
void *resolve_loop(void *data);

