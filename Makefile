CFLAGS = -O2 -Wall -ggdb
OBJS = usg.o dynstuff.o xmalloc.o auth.o msgqueue.o
LIBS = 

all:	usg dirs

usg:	$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o usg

dirs:
	mkdir -p reasons queue passwd

clean:
	rm -f *.o core *~

