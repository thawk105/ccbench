PROG1 = ermia.exe
ERMIA_SRCS1 = ermia.cc transaction.cc util.cc garbage_collection.cc

REL := ../common/
include $(REL)Makefile
ERMIA_ALLSRC = $(ERMIA_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4

CC = g++
CFLAGS = -c -pipe -g -O2 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DMASSTREE_USE=1 \
				 -DADD_ANALYSIS=1 \

ifeq ($(shell hostname), chris41.omni.hpcc.jp)
LDFLAGS = -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7.1.0_libc2.12_kernel2.6.32_release
endif
ifeq ($(shell hostname), dbs11)
LDFLAGS = -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7_libc2.27_kernel4.15.0_release -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7_libc2.27_kernel4.15.0_debug
endif

#LIBS = -lpthread -ltbbmalloc_proxy_debug -ltbbmalloc_debug
LIBS = -lpthread -ltbbmalloc_proxy -ltbbmalloc
ifneq ($(shell hostname), chris41.omni.hpcc.jp)
ifneq ($(shell hostname), dbs11)
LIBS = -lpthread
endif
endif

OBJS1 = $(ERMIA_SRCS1:.cc=.o)

all: $(PROG1)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(ERMIA_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump


