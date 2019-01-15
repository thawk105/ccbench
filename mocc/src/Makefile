PROG = mocc.exe
SOURCES = mocc.cc transaction.cc lock.cc util.cc result.cc

CC = g++
CFLAGS = -c -Wall -g -O2 -std=c++17 -D RWLOCK
#CFLAGS = -c -Wall -g -O2 -std=c++17 -D MQLOCK
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = mocc.o
O2 = lock.o
O3 = result.o
O4 = transaction.o
O5 = util.o

all : $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) $(O5)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $< 
clean:
	rm -f *~ *.o *.exe *.stackdump *.log
