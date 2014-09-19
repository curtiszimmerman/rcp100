#!/bin/bash

#******************************************************
# preserve system files
#******************************************************
if [ -f /etc/hosts ]
then
	cp -a /etc/hosts /opt/rcp/persistent/.
fi
if [ -f /etc/resolv.conf ]
then
	rm -f /opt/rcp/persistent/resolv.conf
	cat /etc/resolv.conf > /opt/rcp/persistent/resolv.conf
	chmod 644 /opt/rcp/persistent/resolv.conf
fi

#******************************************************
# process startup.d directory
#******************************************************
STARTUP_PROCS=`ls /opt/rcp/etc/startup.d`
for file in $STARTUP_PROCS
do
	/opt/rcp/etc/startup.d/$file
done
sleep 1

#******************************************************
# adding bridges
#******************************************************
brctl addbr br0
brctl addbr br1

#******************************************************
# adding additional interfaces
#******************************************************
#DATE=`date`
#echo "$DATE: adding test interfaces"
#ip link add name testing0 type veth peer name donotuse0
#ip link set up donotuse0
#ip link add name testing1 type veth peer name donotuse1
#ip link set up donotuse1

#******************************************************
# starting system
#******************************************************
/opt/rcp/etc/rcpservices &
sleep 2

DATE=`date`
echo "INIT: $DATE: starting RCP processes"
INITPROCS=`ls /opt/rcp/etc/init.d`
for file in $INITPROCS
do
	/opt/rcp/etc/init.d/$file &
done
sleep 2
/opt/rcp/bin/rcpwait

#******************************************************
# run startup configuration
#******************************************************

DATE=`date`
echo "INIT: $DATE: configuring system"
if [ -f /home/rcp/startup-config ]
then
	/opt/rcp/bin/cli --script /home/rcp/startup-config
else
	/opt/rcp/bin/cli --script /opt/rcp/etc/default-config
fi
/opt/rcp/bin/rcpxmlstats --load
	
DATE=`date`
echo "INIT: $DATE: RCP system started"

