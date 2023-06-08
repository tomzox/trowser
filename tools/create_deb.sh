#!/bin/bash

set -e

BASEDIR=deb/trowser_1.5

mkdir -p $BASEDIR/DEBIAN
cat > $BASEDIR/DEBIAN/control <<EoF
Package: trowser
Priority: optional
Section: text
Maintainer: T. Zoerner <tomzox@gmail.com>
Architecture: all
Version: 1.5
Depends: tcl8.4|tcl8.5|tcl8.6, tk8.4|tk8.5|tk8.6
Description: Browser for large line-oriented text files based on Tcl/Tk
 Trowser is a browser for large line-oriented text files (such as debug traces)
 implemented in Tcl/Tk.  It's meant as an alternative to "less". Compared
 to less, trowser adds color highlighting, a persistent search history,
 graphical bookmarking, separate search result (i.e. filter) windows and
 flexible skipping of input from pipes to STDIN.  Trowser has a graphical
 interface, but is designed to allow browsing via the keyboard at least
 to the same extent as less. Key bindings and the cursor positioning
 concept are derived from vim.
EoF

mkdir -p $BASEDIR/usr/bin
cp -p trowser.tcl $BASEDIR/usr/bin/trowser
chmod +x $BASEDIR/usr/bin/trowser

mkdir -p $BASEDIR/usr/share/man/man1
gzip -n -9 -c doc/trowser.1 > $BASEDIR/usr/share/man/man1/trowser.1.gz

mkdir -p $BASEDIR/usr/share/doc/trowser
cp -p README.md $BASEDIR/usr/share/doc/trowser/README.txt
gzip -n -9 -c doc/changelog-tcl.txt > $BASEDIR/usr/share/doc/trowser/changelog.gz
gzip -n -9 -c doc/trowser.pod > $BASEDIR/usr/share/doc/trowser/trowser.pod.gz

cat > $BASEDIR/usr/share/doc/trowser/copyright <<EoF
Copyright (C) 2007-2009,2019-2020,2023 T. Zoerner. All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

For a copy of the GNU General Public License see:
/usr/share/common-licenses/GPL-3

Alternatively, see <http://www.gnu.org/licenses/>.
EoF

cd $BASEDIR
md5sum  usr/share/doc/trowser/copyright \
	usr/share/doc/trowser/changelog.gz \
	usr/share/doc/trowser/README.txt \
	usr/share/doc/trowser/trowser.pod.gz \
	usr/share/man/man1/trowser.1.gz \
	usr/bin/trowser \
      > DEBIAN/md5sums
cd ..

# note "fakeroot" is used to have files owned by user "root"
# - this is optional for local packages; can be removed if you don't have this script

fakeroot dpkg-deb --build trowser_1.5
