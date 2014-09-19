all: tftp

include ../MCONFIG
include ../MRULES

OBJS = tftp.o main.o tftpsubs.o

tftp: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(OBJS): tftpsubs.h
tftp.o: ../version.h

install: tftp
	install -s -m$(BINMODE) tftp $(INSTALLROOT)$(BINDIR)
	install -m$(MANMODE) tftp.1 $(INSTALLROOT)$(MANDIR)/man1

clean:
	rm -f *.o tftp

