#!/bin/bash

DATE=`date`
echo "INIT: $DATE: shuting down RCP system"

#bring down all rcp processes
pkill rcpservices

INITPROCS=`ls /opt/rcp/etc/init.d`
for file in $INITPROCS
do
	pkill $file
done

pkill xinetd
pkill tftpd
pkill ntpd
pkill cli
pkill snmpd

#unmount var/transport directory
TRANSPORT=`grep transport /etc/mtab`
if [ "$?" -eq 0 ];
then
    umount /opt/rcp/var/transport
fi

# remove tunnel interfaces
#ip link delete testing0
#ip link delete testing1

rm -f /dev/shm/rcp*
rm -f /dev/shm/sem.rcp.sem
rm -f /dev/shm/sem.rcphttp.sem
rm -f /var/tmp/rcpsocket

LOGFILES=`ls /opt/rcp/var/log`
for file in $LOGFILES
do
	mv /opt/rcp/var/log/$file /opt/rcp/persistent/.
done
rm -f /opt/rcp/etc/xinetd.d/*
rm -f /opt/rcp/etc/ntpd.conf
rm -fr /opt/rcp/var/net-snmp


#******************************************************
# restore system files
#******************************************************
if [ -f /opt/rcp/persistent/hosts ]
then
	rm -f /etc/hosts
	cp -a /opt/rcp/persistent/hosts /etc/.
fi
if [ -f /opt/rcp/persistent/resolv.conf ]
then
	rm -f /etc/resolv.conf
	cp -a /opt/rcp/persistent/resolv.conf /etc/.
fi

DATE=`date`
echo "INIT: $DATE: RCP system stopped"
