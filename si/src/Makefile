PROG = si.exe
SOURCES = si.cc transaction.cc util.cc garbageCollection.cc result.cc

CC = g++
CFLAGS = -c -Wall -g -O2 -std=c++17
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = si.o
O2 = transaction.o
O3 = util.o
O4 = garbageCollection.o
O5 = result.o

all: $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) $(O5)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump


