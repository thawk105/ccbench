PROG = mocc.exe
SOURCES = mocc.cc makeDB.cc makeProcedure.cc transaction.cc stopWatch.cc displayGlobal.cc lock.cc

CC = g++
#CFLAGS = -c -Wall -g -O2 -std=c++11 -D RWLOCK
CFLAGS = -c -Wall -g -O2 -std=c++11 -D MQLOCK
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = mocc.o
O2 = makeDB.o
O3 = makeProcedure.o
O4 = transaction.o
O5 = stopWatch.o
O6 = displayGlobal.o
O7 = lock.o

all : $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) $(O5) $(O6) $(O7)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $< 
clean:
	rm -f *~ *.o *.exe *.stackdump *.log
