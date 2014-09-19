#!/bin/bash
#**********************************
# build Ubuntu package
#**********************************

ARCHIVE_SRC="rcp100-$1.tar.bz2"
ARCHIVE_BIN="rcp100-$1.bin.tar.bz2"
ARCHIVE_DEB="rcp100_$1_1.deb"
rm $ARCHIVE_SRC
rm $ARCHIVE_BIN
rm $ARCHIVE_DEB
rm testing/smoketest/debian.deb

# build binary
./buildtools/mkdist-binary.sh $1
rm $ARCHIVE_SRC

# build debian directory
mkdir -p debian/DEBIAN
mkdir -p debian/opt
tar -xjvf $ARCHIVE_BIN
mv rcp debian/opt/.
rm $ARCHIVE_BIN

# add Debian control files
mkdir -p debian/usr/share/doc/rcp100
mv debian/opt/rcp/usr/share/doc/* debian/usr/share/doc/rcp100/.
rm debian/usr/share/doc/rcp100/COPYING
mv debian/usr/share/doc/rcp100/RELNOTES debian/usr/share/doc/rcp100/changelog.Debian
gzip -9 debian/usr/share/doc/rcp100/changelog.Debian
cp buildtools/debian-copyright  debian/usr/share/doc/rcp100/copyright
# cp buildtools/debian-control debian/DEBIAN/control
sed "s/RCPVER/$1/g"  buildtools/debian-control > debian/DEBIAN/control
cp buildtools/debian-postrm debian/DEBIAN/postrm
mkdir -p debian/etc/init
cp buildtools/debian-rcp100.conf debian/etc/init/rcp100.conf
cp buildtools/debian-conffiles debian/DEBIAN/conffiles
find ./debian -type d | xargs chmod 755

# build the package
dpkg-deb --build debian
lintian debian.deb | grep -v dir-or-file-in-opt
cp debian.deb rcp100_$1_1.deb
mv debian.deb testing/smoketest/.
rm -fr debian

