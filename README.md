#RCP100

Written by RCPTeam (rcpteam at yahoo dot com) [Project Website](http://rcp100.sourceforge.net/)

###Installing RCP100 using the source code package

RCP100 software should compile and run on any recent Linux 64bit system. Before you start, you need to install a number of additional packages:

```
Fedora
CentOS 6.x	
yum install gcc-c++ ncurses-devel bridge-utils rsync curl traceroute lsof
Ubuntu
Debian	
apt-get update
apt-get install build-essential ncurses-dev bridge-utils rsync curl traceroute lsof
openSUSE	
yast2 -i  gcc-c++ make ncurses-devel bridge-utils rsync curl tcptraceroute lsof
```

Download the latest RCP100 version and unpack the archive:

```bash
$ tar -xjvf rcp100-X.Y.Z.tar.bz2
$ cd rcp100-X.Y.Z
```

Compile and install the software:

```bash
$ ./configure
$ make
$ sudo make install
# exit
```

The installation takes place in /opt/rcp. No other directory is modified.

###Starting the software

Run /opt/rcp/bin/start.sh script to start the router:

```bash
$ sudo /opt/rcp/bin/start.sh
INIT: Mon Apr  9 11:51:39 EDT 2012: init user account
INIT: Mon Apr  9 11:51:39 EDT 2012: mount transport directory
INIT: Mon Apr  9 11:51:39 EDT 2012: setting up system defaults
INIT: Mon Apr  9 11:51:42 EDT 2012: starting RCP processes
INIT: Mon Apr  9 11:51:44 EDT 2012: configuring system
CLI: Executing command: hostname rcp
CLI: Executing command: administrator rcp password rcp
CLI: Executing command: service telnet
INIT: Mon Apr  9 11:51:44 EDT 2012: RCP system started
$
```

The script creates a new unprivileged user account in /home/rcp, and the router is started with a minimal configuration. Existing interfaces are detected, and two bridge interfaces - br0 and br1 - are created. You can use these interfaces to hook up different virtual machines running on the same computer.

###Accessing the router using the web interface

Point your browser to http://0/index.html and login as user rcp, password rcp:

```
RCP100 CLI screenshot
```

Next, you will be asked to change the default passwords and proceed to the configuration section:

```
RCP100 CLI screenshot
```

###Accessing the router using the command line interface

Log into Command Line Interface (CLI) subsystem via telnet as user rcp, password rcp:

```bash
$ telnet 0
Trying 0.0.0.0...
Connected to 0.
Escape character is '^]'.
User: rcp
Password: 
rcp>
```

CLI is very similar to what you find in commercial routers. It supports command completion (TAB key), syntax checking, command abbreviation, context sensitive help ('?' key) and session history (show history command).

```bash
rcp>?
  enable                      Administration mode
  exit                        Exit the current mode
  logout                      Exit the session
  no                          Negate a command or set its defaults
  ping                        Send echo messages
  show                        Show running system information
  telnet-client               Open a telnet session
  traceroute                  Trace route to destination
rcp>
```

Go into configuration mode and changed the default password:

```bash
rcp>en
rcp#config
rcp(config)#administrator rcp password mysecretpasswd
rcp(config)#exit
rcp#
```

Use show running-config command to inspect the current running configuration:

```bash
rcp#show running-config
hostname rcp
!
service telnet
service http
!
administrator rcp encrypted password QFSNKPHJ$kNBAqXTxlxZhwKWIpt1e61
!
!
interface loopback lo
  ip address 127.0.0.1/8
  ip mtu 16436
  no shutdown
!
interface ethernet eth0
  ip address 192.168.254.19/24
  ip mtu 1500
  no shutdown
!
interface bridge br0
  ip mtu 1500
  shutdown
!
interface bridge br1
  ip mtu 1500
  shutdown
!
rcp#
```

The configuration is saved using copy running-config startup-config command:

```bash
rcp#copy running-config startup-config 
rcp#
```

or abbreviated:

```bash
rcp#copy run start
rcp#
```

###Stopping the software

Stop the software by running /opt/rcp/bin/stop.sh script as root. The next restart will apply the saved configuration.

```bash
$ sudo /opt/rcp/bin/stop.sh 
INIT: Thu Feb 14 08:32:04 EST 2013: shuting down RCP system
INIT: Thu Feb 14 08:32:04 EST 2013: RCP system stopped
$
```
