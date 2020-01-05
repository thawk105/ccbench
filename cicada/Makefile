PROG1 = cicada.exe
CICADA_SRCS1 := cicada.cc transaction.cc util.cc

REL := ../common/
include $(REL)Makefile
CICADA_ALLSRC = $(CICADA_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4
ADD_ANALYSIS=1
BACK_OFF=0
INLINE_VERSION_PROMOTION=1
INLINE_VERSION_OPT=1
MASSTREE_USE=1
PARTITION_TABLE=1
REUSE_VERSION=1
SINGLE_EXEC=0
WRITE_LATEST_ONLY=0

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DKEY_SIZE=$(KEY_SIZE) \
				 -DVAL_SIZE=$(VAL_SIZE) \
				 -DADD_ANALYSIS=$(ADD_ANALYSIS) \
				 -DBACK_OFF=$(BACK_OFF) \
				 -DINLINE_VERSION_PROMOTION=$(INLINE_VERSION_PROMOTION) \
				 -DINLINE_VERSION_OPT=$(INLINE_VERSION_OPT) \
				 -DMASSTREE_USE=$(MASSTREE_USE) \
				 -DPARTITION_TABLE=$(PARTITION_TABLE) \
				 -DREUSE_VERSION=$(REUSE_VERSION) \
				 -DSINGLE_EXEC=$(SINGLE_EXEC) \
				 -DWRITE_LATEST_ONLY=$(WRITE_LATEST_ONLY) \

# WRITE_LATEST_ONLY : for all write. 
# original model adapt write-latest-only-rule for RMW
# , not for blind write.

# SINGLE_EXEC option can be executed 
# only non conflict workload.
# It must use PARTITION_TABLE for dividing 
# table to realize non conflict workload.
# If SINGLE_EXEC, it must use INLINE_VERSION_OPT.
# Because SINGLE_EXEC mode use inline version.

LDFLAGS = -L../third_party/mimalloc/out/release \
					-L../third_party/mimalloc/out/debug \
					-L../third_party/tbb/build/linux_intel64_gcc_cc7.4.0_libc2.27_kernel4.15.0_release \

LIBS = -lpthread -lmimalloc

OBJS1 = $(CICADA_SRCS1:.cc=.o)

all: $(PROG1)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(CICADA_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
