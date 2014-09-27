EXTRA_DIST = gtkpinger pinger.1

bin_PROGRAMS = pinger
man_MANS = pinger.1
bin_SCRIPTS = gtkpinger

  P_SOURCES = timefunc.c parse.c ping.c pinger.c dns.c log.c timefunc.h parse.h ping.h globals.h fake_gtk.h dns.h log.h
if HAVE_CURSES
  P_SOURCES_CURSES = interface_ncurses.c interface_ncurses.h
else
  P_SOURCES_CURSES = 
endif
if ENABLE_ARP
  P_SOURCES_ARP = arp.c arp.h
else
  P_SOURCES_ARP =
endif
if HAVE_GTK_2
  P_SOURCES_GTK = interface_gtk.c interface_gtk.h
else
  P_SOURCES_GTK = 
endif
if HAVE_GTK_3
  P_SOURCES_GTK3 = interface_gtk.c interface_gtk.h
else
  P_SOURCES_GTK3 = 
endif
pinger_SOURCES = $(P_SOURCES) $(P_SOURCES_CURSES) $(P_SOURCES_GTK) $(P_SOURCES_GTK3) $(P_SOURCES_ARP)

install-exec-hook:
	if [ `id | cut -c7-10` != "root" ]; then echo -e "\nRoot privileges are NECCESSARY during installation !!!\n"; exit 1; fi
	chmod u+s $(DESTDIR)$(bindir)/pinger
