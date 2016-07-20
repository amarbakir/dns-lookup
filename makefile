# Authors: Amar Bakir, Shashank Seeram
# CS 352

COMPILER = gcc
CCFLAGS = -g
all: dns-server

run:
	./dns-server

debug:
	make DEBUG=TRUE

nodebug:
	make DEBUG=FALSE

dns-server: dns-server.o
	$(COMPILER) $(CCFLAGS) -o dns-server dns-server.o

dns-server.o: dns-server.c
	$(COMPILER) $(CCFLAGS) -c dns-server.c

ifeq ($(DEBUG), TRUE)
 CCFLAGS += -g
endif

clean:
	rm -f dns-server
	rm -f *.o
