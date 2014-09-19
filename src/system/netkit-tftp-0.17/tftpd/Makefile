# Added RCP support
all: tftpd

include ../MCONFIG
include ../MRULES

OBJS = tftpd.o tftpsubs.o

tftpd: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) ../../../librcp/librcp.a -lpthread -o $@ -lrt

tftpsubs.c: 
	ln -sf ../tftp/tftpsubs.c .
tftpsubs.h: 
	ln -sf ../tftp/tftpsubs.h .

$(OBJS): tftpsubs.h
tftpd.o: ../version.h

install: tftpd
	install -s -m$(DAEMONMODE) tftpd $(INSTALLROOT)$(SBINDIR)/in.tftpd
	install -m$(MANMODE) tftpd.8 $(INSTALLROOT)$(MANDIR)/man8/in.tftpd.8
	ln -sf in.tftpd.8 $(INSTALLROOT)$(MANDIR)/man8/tftpd.8

clean:
	rm -f *.o tftpd tftpsubs.c tftpsubs.h

