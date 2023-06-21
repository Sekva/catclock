SRCS = xclock.c
OBJS = xclock.o

XLIB      = -lX11
MOTIFLIBS = -lXm -lXt
EXTENSIONLIB = -lXext
SYSLIBS   = -lm
LIBS      = $(MOTIFLIBS) $(EXTENSIONLIB) $(XLIB) $(SYSLIBS)

INCS      = -I.

CDEBUGFLAGS = -ggdb
CFLAGS      = $(DEFINES) $(INCS) $(CDEBUGFLAGS)

PROG  = xclock

.c.o:
	$(CC) -c $(INCS) $(CFLAGS) $*.c

all: $(PROG)

$(PROG): $(SRCS) Makefile
	$(CC) -o $(PROG) $(CFLAGS) $(SRCS) $(LIBS)

clean:
	rm -f *.o $(PROG)
