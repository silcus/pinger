# PINGER - GTK+/ncurses multi-ping utility
Copyright (C) 2002-2017 Petr Koloros <[namesurname]@gmail.com>
See AUTHORS for more.

This program GPL'ed. See COPYING for more details.

## What is pinger

Imagine a situation when you want to see if other machines are running (very
often in you neighborhood or subnet). Pinger allows you to see status of many
hosts at once with their response times using standard ICMP messages or ARP
requests. You can easily get informations about network congestion or dead
parts.

## Requirements

One of these (if both are available, it's cool):
- GTK+ 2.x and better
  - http://www.gtk.org
- Ncurses (tested with version 5.0 and higher)
  - http://www.gnu.org/software/ncurses

Since version 0.3 pinger is more modular (and readable :-)) so you can add your
favorite interface if you are encouraged code writer.
      
## Installation

> ./configure
> make
> make install

It needs to be a 'suid' application, so `make install` as root. It should
make 'suid' executable by itself.

Default font is font you have set as default for gtk applications. If you want to change it, uncomment this line 

> //#define USE_FONT_FAMILY

in file <top directory>/src/pinger.c and set the font family in the line
below if you want.

## Debian package

Since version 0.32 pinger distribution is capable of creating the debian
package. There are two steps needed:

1. Configure pinger for debian directory structure, i486 and with stripping
debug info:
> LDFLAGS="-s" CFLAGS="-O2 -march=i486" ./configure --prefix=/usr
2. Create debian package by typing this (under root of course):
> make deb

It should produce package like pinger_0.32-1_i386.deb. It is very basic
package. I don't use Debian/Ubuntu/*deb* on my laptop, so if you encounter any
error, please let me know so I can fix it.

Note: if you like some other destination you want pinger to stay, use
--prefix=myfavoritedirectory. Documentation will be placed automatically in the
/usr/share/doc/pinger-version directory.

## Usage

Currently pinger has two modes:

- pinger - starts pinger in text console using ncurses interface.
- gtkpinger - (or pinger --gtk) use gtk as graphical interface

Both version are equally featured. Quit is done by pressing of any key.

## Configuration

Pinger uses configuration file which is by default stored in your home
directory. If you don't have any and want the pinger to create some for you,
just run pinger. It is a good move too when you just upgraded. Please see
Upgrade section for more details.

Pinger can use multiple configuration files at once:

./pinger configuration-file configuration-file2 configuration-file3 ..

They will be arranged into "tabs" in the pinger's interface. PgDn and PgUp
allow you to switch between them.

Multiple configuration files doesn't inherit settings between themselves. For
example if you want logging of all sets of hosts, you must set that option in
all configuration files. It brings advantage of combining variety of sets in
different ways and order.

Creating a configuration file is rather easy. Use the example file which pinger
creates for you (in your home directory, named .pingerrc) when executed for the
first time. Please stick with the format it uses. Pinger controls if anything is
wrong. In case something is not written as pinger expected, it quits
immediately. Sometimes it may not be satisfied with values you typed there (out
of range which pinger considers as sane) and then it will tell you and set the
default value. You can use error log file to log the errors which pinger
encounters. See chapter Logging.

## Upgrade

When upgrading from previous version, your configuration file should work as
before. But new versions also may bring some new options you can use. If you
want to know them, backup your old configuration file (~/.pingerrc) and run
pinger so it can't find it in your home directory. It creates a new one where
you can check new options inside. Then you can restore your previous
configuration file and add options found in new one.

## 80 characters terminal

There is not enough space to display all featured column. I call these bonus
columns and they are displayed only in wider terminals or in graphics mode
(GTK).

Currently there is only "Last OK" column considered as bonus column. It shows
timestamp of last successful ping for each destination.

## Timing

There are customizable times which set host status. There are described in the
configuration file.
Briefly:
- host_timeout_ms - if there is no answer in that time, host is considered offline and new request is send to the host. If pings are successful
- delay_between_pings_ms is used to wait before sending another request to this host
- long_delay_ms means that host responded in time, but this response is quite delayed (says something like warning, connection to this host is really slow)
- refresh_interval_ms otherwise means how often should be the user interface updated. If you set this for example to 5000ms, you are going to see the results every 5 seconds. Meanwhile results are frozen while pinger is doing pinging in the background.

# Domain names resolving

Since version 0.31 pinger resolves names specified using domain name format
(put it in the configuration file instead of IP). It resolves all those names
periodically with interval 10 minutes or whatever you specify in configuration
file (dns_check_s option). If name has no resolve result but has been resolved
before, the previous value is kept.

# ARP pinging

Some hosts may not respond to the ICMP requests (classic ping mechanism). On
the local network there may be better sometimes to use ARP instead of ICMP
messages (especially for machines running Windows operating system), because
ARP cannot be easily blocked from the client. The use of ARP must be specified
in the configuration file for each host. (Put the "arp" after host name like
this: IP/domain=name,arp[,params])

# Logging

Pinger supports two types of logging - error log and host log.

Error log can be used for error messages. If not specified, error messages are printed on
standard error output in terminal (you can redirect it by adding " 2> file" in
the command line when executing pinger.

Host log is for host changes. With the global parameter log_if_status_lasts_s you can set how long must status of the
host lasts in order to be logged into file.

It helps to log only important status changes and ignore the minor ones. If you don't specify log files (using parameters error_log or host_log), logging will be ignored. Pinger tries to guess the best logging alternative. If you specify some hosts to log and the rest are unspecified, it will not log them. If you specify some hosts to not log, the rest will be logged. If you specify to log some host and to not log some other hosts, default value (log_if_status_last_s) will be used for the rest (if it is set to -1, it means to not log. Otherwise they will be logged). If you don't specify log_if_status_last_s parameter, default value is 0, which means to log every change.

