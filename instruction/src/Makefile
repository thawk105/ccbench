PROG = fetch_add.exe
PROG2 = xoroshiro.exe

CC = g++
CFLAGS = -c -Wall -g -O2 -std=c++11
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = stopWatch.o
O2 = fetch_add.o
O3 = xoroshiro.o

all: $(PROG) $(PROG2)

$(PROG) : $(O1) $(O2)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
$(PROG2) : $(O1) $(O3)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump
