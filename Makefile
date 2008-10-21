CFLAGS = -O2 -Wall -ggdb
OBJS = usg.o dynstuff.o xmalloc.o auth.o msgqueue.o protocol.o
LIBS = -lssl

all:	usg dirs

usg:	$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o usg

dirs:
	mkdir -p reasons queue
	touch passwd

clean:
	rm -f usg *.o core *~

