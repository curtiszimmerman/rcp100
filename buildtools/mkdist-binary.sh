#!/bin/bash
#**********************************
# build executable package
#**********************************
# check arguments
if [ $# -ne 1 ]
then
    echo "Error: program argument missing"
    echo "Usage: $0 version"
    exit 1
fi

# check for chrpath
if which chrpath >/dev/null
then
    echo chrpath executable found
else
    echo chrpath program not found
    exit 1
fi

buildtools/mkdist.sh $1

ARCHIVE_SRC="rcp100-$1.tar.bz2"
ARCHDIR="rcp100-$1"

rm testing/smoketest/binary.tar.bz2
tar -xjvf $ARCHIVE_SRC
cd $ARCHDIR
pwd

./configure 
make -j2 install
cp -a /opt/rcp rcp
strip rcp/sbin/*
strip rcp/bin/*
strip rcp/usr/bin/*
chrpath --replace /opt/rcp/lib rcp/lib/*.so
tar -cjvf ../binary.tar.bz2 rcp
cd ..
cp binary.tar.bz2 rcp100-$1.bin.tar.bz2
mv binary.tar.bz2 testing/smoketest/.
rm -fr $ARCHDIR

