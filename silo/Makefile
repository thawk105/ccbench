PROG1 = silo.exe
SILO_SRCS1 := silo.cc transaction.cc util.cc 
PROG2 = replayTest.exe
SRCS2 := replayTest.cc

REL := ../common/
include $(REL)Makefile

KEY_SIZE=4
VAL_SIZE=4

CC = g++
BASE_CFLAGS = -c -Wall -Wextra -Wdangling-else -g -O2 -std=c++17 -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) -D$(shell uname)
LINUX_CFLAGS = -Wchkp -Winvalid-memory-model

CFLAGS = $(BASE_CFLAGS)
ifeq ($(shell uname), Linux)
CFLAGS = $(BASE_CFLAGS) $(LINUX_CFLAGS)
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

OBJS1 = $(SILO_SRCS1:.cc=.o)
OBJS2 = $(SRCS2:.cc=.o)

all: $(PROG1) $(PROG2)

$(PROG1) : $(OBJS1)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PROG2) : $(OBJS2) 
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
