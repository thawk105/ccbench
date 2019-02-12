PROG = fetch_add.exe
PROG2 = xoroshiro.exe

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
