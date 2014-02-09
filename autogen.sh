#!/bin/sh
echo Add gettextize infrastructure.
autopoint -f || {
echo "It seems you don't have required gettext version installed."
echo "Please install the one metioned above or change AM_GNU_GETTEXT_VERSION"
echo "macro inside configure.in file."
echo
exit 1
}
echo Running aclocal.
aclocal -I m4 $ACLOCAL_FLAGS || 
{
echo
echo Try to set ACLOCAL_FLAGS environment variable, if you haven\'t done so.
echo You should specify, where to search required aclocal files. An example
echo should look like this: export ACLOCAL_FLAGS=\"-I /opt/gtk/share/aclocal\"
echo
exit 1
}
echo Running autoheader.
autoheader
echo Running automake.
automake -a
echo Running autoconf.
autoconf
echo Running configure.
./configure $@

