PROG = mocc.exe
SOURCES = mocc.cc transaction.cc lock.cc util.cc result.cc

KEY_SIZE=4
VAL_SIZE=4

CC = g++
BASE_CFLAGS = -c -Wall -Wextra -Wdangling-else -g -O2 -std=c++17 -DRWLOCK -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) -D$(shell uname)
LINUX_CFLAGS = -Wchkp -Winvalid-memory-model 
TYPE = -DDEBUG

CFLAGS = $(BASE_CFLAGS)
ifeq ($(shell uname), Linux)
CFLAGS = $(BASE_CFLAGS) $(LINUX_CFLAGS) $(TYPE)
endif

ifeq ($(shell hostname), chris41.omni.hpcc.jp)
LDFLAGS = -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7.1.0_libc2.12_kernel2.6.32_release
endif
ifeq ($(shell hostname), dbs11)
LDFLAGS = -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7_libc2.27_kernel4.15.0_release
endif

LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc
ifneq ($(shell hostname), chris41.omni.hpcc.jp)
ifneq ($(shell hostname), dbs11)
LIBS = -lpthread
endif
endif

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
