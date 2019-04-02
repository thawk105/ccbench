PROG1 = fetch_add.exe
INST_SRCS1 = fetch_add.cc

PROG2 = xoroshiro.exe
INST_SRCS2 = xoroshiro.cc

PROG3 = cache-test.exe
INST_SRCS3 = cache-test.cc

PROG4 = membench.exe
INST_SRCS4 = membench.cc

PROG5 = xoroshiro2.exe
INST_SRCS5 = xoroshiro2.cc

REL := ../common/
include $(REL)Makefile

CC = g++
CFLAGS = -c -Wall -Wextra -Wchkp -Winvalid-memory-model -Wdangling-else -g -O2 -std=c++17 -D$(shell uname)

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

OBJS1 = $(INST_SRCS1:.cc=.o)
OBJS2 = $(INST_SRCS2:.cc=.o)
OBJS3 = $(INST_SRCS3:.cc=.o)
OBJS4 = $(INST_SRCS4:.cc=.o)
OBJS5 = $(INST_SRCS5:.cc=.o)

all: $(PROG1) $(PROG2) $(PROG3) $(PROG4) $(PROG5)

$(PROG1) : $(OBJS1)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PROG2) : $(OBJS2)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PROG3) : $(OBJS3)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PROG4) : $(OBJS4)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PROG5) : $(OBJS5)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
