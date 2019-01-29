PROG = cicada.exe
SOURCES = cicada.cc transaction.cc util.cc result.cc

CC = g++
CFLAGS = -c -Wall -Wextra -Wchkp -Winvalid-memory-model -Wdangling-else -g -O2 -std=c++17 
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = cicada.o
O2 = transaction.o
O3 = util.o
O4 = result.o

all: $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) 
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump
