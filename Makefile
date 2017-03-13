# Generated automatically from Makefile.in by configure.
#
# Makefile for libminiserver
#

srcdir          = .

install_prefix  =
prefix          = /usr/local
exec_prefix	= ${prefix}
libdir		= ${exec_prefix}/lib
mandir		= ${prefix}/man
includedir	= ${prefix}/include

CC	= gcc
DEFS	=  -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYS_QUEUE_H=1 -DTIME_WITH_SYS_TIME=1 -DHAVE_GETTIMEOFDAY=1 -DHAVE_EPOLL=1 
CFLAGS	= -g -O2 $(DEFS) -Wall -I.
LDFLAGS	= 

INSTALL	= /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644

HDRS	= event.h miniserver.h miniconfig.h whttpparse.h
SRCS	= event.c epoll.c select.c miniserver.c whttpparse.c
OBJS	= $(SRCS:.c=.o)

.c.o:
	$(CC) $(CFLAGS) -c $(srcdir)/$*.c

all: libminiserver.a

libminiserver.a: $(OBJS)
	@echo "building $@"
	ar cru $@ $(OBJS)
	ranlib $@

install:
	$(INSTALL) -d $(install_prefix)$(libdir)
	$(INSTALL_DATA) libminiserver.a $(libdir)
	$(INSTALL) -d $(install_prefix)$(mandir)/man3
	$(INSTALL_DATA) *.3 $(install_prefix)$(mandir)/man3
	$(INSTALL) -d $(install_prefix)$(includedir)
	$(INSTALL_DATA) *.h $(install_prefix)$(includedir)

clean:
	rm -f *.o *~ libminiserver.a

distclean: clean
	rm -f Makefile config.h config.cache config.log config.status

# EOF
