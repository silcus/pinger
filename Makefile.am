SUBDIRS = m4 po src 

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath mkinstalldirs  autogen.sh BUGS po/README deb/control/control \
deb/debian-binary deb/doc.list deb/postinst.in

deb: $(distdir).deb

$(distdir).deb:
	if ! make; then \
	echo; \
	echo "Errors during make. Fix above and try again.";  \
	echo; \
	exit 1; \
	fi

	debpkgname=$(PACKAGE)_$(VERSION)-1_i386.deb; \
	rm $$debpkgname; \
	curdir=`pwd`; \
	if [ "x$$curdir" == "x" ]; then \
	  echo "Cannot stat current dir!"; \
	  exit 1;\
	fi; \
	echo $$curdir; \
	debdir=$$curdir"/deb"; \
	datadir=$$debdir"/data"; \
	if [ -x $$datadir ] && ls $$datadir/* >/dev/null 2>&1; then \
		rm -r $$debdir/data/*; \
	fi; \
	sed 's%PINGERBINDIR%$(bindir)%g' $$debdir/postinst.in > $$debdir/control/postinst; \
	chmod a+x $$debdir/control/postinst; \
	if ! DESTDIR=$$datadir make install; then \
	echo; \
	echo "Errors during fake installaton. Fix above and try again."; \
	echo; \
	exit 1; \
	fi; \
	if [ -d $$datadir/${prefix}/man ]; then \
	  mv $$datadir/${prefix}/man $$datadir/${prefix}/share/man; \
	fi; \
	install -d $$datadir/usr/share/doc/$(distdir); \
	for a in `cat $$debdir/doc.list`; do cp $$a $$datadir/usr/share/doc/$(distdir); done; \
	cd $$debdir/control; \
	tar czvf $$debdir/control.tar.gz *; \
	cd $$datadir; \
	tar czvf $$debdir/data.tar.gz *; \
	cd $$debdir; \
	ar r ../$$debpkgname debian-binary control.tar.gz data.tar.gz 
