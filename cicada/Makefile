PROG = cicada.exe
SOURCES = cicada.cc transaction.cc util.cc result.cc

CC = g++
CFLAGS = -c -Wall -Wextra -Wchkp -Winvalid-memory-model -Wdangling-else -g -O2 -std=c++17 

ifeq ($(shell hostname), chris41.omni.hpcc.jp)
LDFLAGS = -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7.1.0_libc2.12_kernel2.6.32_release
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc
endif
ifeq ($(shell hostname), dbs11)
LDFLAGS = -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7_libc2.27_kernel4.15.0_release
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc
endif

ifneq ($(shell hostname), chris41.omni.hpcc.jp)
ifneq ($(shell hostname), dbs11)
LIBS = -lpthread
endif
endif

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
