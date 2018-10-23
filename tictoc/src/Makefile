PROG = tictoc.exe
SOURCES = tictoc.cc makeDB.cc makeProcedure.cc transaction.cc util.cc displayGlobal.cc

CC = g++
#CFLAGS = -c -Wall -g -O2 -std=c++11 -march=native
# i find the case that using march-native degrade performance.
CFLAGS = -c -Wall -g -O2 -std=c++11
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = tictoc.o
O2 = makeDB.o
O3 = makeProcedure.o
O4 = transaction.o
O5 = util.o
O6 = displayGlobal.o

all: $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) $(O5) $(O6)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump
