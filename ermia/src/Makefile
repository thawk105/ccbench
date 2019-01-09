PROG = ermia.exe
SOURCES = ermia.cc transaction.cc util.cc

CC = g++
CFLAGS = -c -Wall -g -O2 -std=c++11
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = ermia.o
O2 = transaction.o
O3 = util.o

all: $(PROG)

$(PROG) : $(O1) $(O2) $(O3) 
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump


