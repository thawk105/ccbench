PROG = ss2pl.exe
SOURCES = ss2pl.cc makeDB.cc makeProcedure.cc displayGlobal.cc transaction.cc stopWatch.cc

CC = g++
CFLAGS = -c -Wall -g -O2 -std=c++11
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy

O1 = ss2pl.o
O2 = makeDB.o
O3 = makeProcedure.o
O4 = displayGlobal.o
O5 = transaction.o
O6 = stopWatch.o

all: $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) $(O5) $(O6)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump


