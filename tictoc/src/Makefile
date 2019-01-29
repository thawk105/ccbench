PROG = tictoc.exe
SOURCES = tictoc.cc transaction.cc result.cc util.cc 

CC = g++
#CFLAGS = -c -Wall -g -O2 -std=c++17 -march=native
# i find the case that using march-native degrade performance.
CFLAGS = -c -Wall -Wextra -Wchkp -Winvalid-memory-model -Wdangling-else -g -O2 -std=c++17 
LDFLAGS = -L./../lib
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc

O1 = tictoc.o
O2 = transaction.o
O3 = result.o
O4 = util.o

all: $(PROG)

$(PROG) : $(O1) $(O2) $(O3) $(O4) 
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
.cc.o:
	$(CC) $(CFLAGS) $<
clean:
	rm -f *~ *.o *.exe *.stackdump
