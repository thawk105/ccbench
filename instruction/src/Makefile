PROG = fetch_add.exe
SOURCES = fetch_add.cc stopWatch.cc

CC = g++
CFLAGS = -c -Wall -g -O2 -std=c++11
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = fetch_add.o
O2 = stopWatch.o

all: $(PROG)

$(PROG) : $(O1) $(O2)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump


