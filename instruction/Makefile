PROG1 = fetch_add
INST_SRCS1 = fetch_add.cc

PROG2 = cache-test
INST_SRCS2 = cache-test.cc

PROG3 = membench
INST_SRCS3 = membench.cc

PROG4 = rdtscBench
INST_SRCS4 = rdtscBench.cc

PROG5 = xoroshiro
INST_SRCS5 = xoroshiro.cc

REL := ../common/
include $(REL)Makefile

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wchkp -Winvalid-memory-model -Wdangling-else \
				 -D$(shell uname) \
				 -D$(shell hostname) \

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

$(PROG1) : $(OBJS1) ../common/util.o
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
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(wildcard *.cc)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
