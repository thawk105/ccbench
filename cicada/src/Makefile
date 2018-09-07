PROG = cicada.exe
SOURCES = cicada.cc makeDB.cc makeProcedure.cc mutex.cc transaction.cc stopWatch.cc displayGlobal.cc

CC = g++
CFLAGS = -c -Wall -g -O2 -std=c++11 -march=native
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = cicada.o
O2 = makeDB.o
O3 = makeProcedure.o
O4 = mutex.o
O5 = transaction.o
O6 = stopWatch.o
O7 = displayGlobal.o

all: $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) $(O5) $(O6) $(O7)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump
