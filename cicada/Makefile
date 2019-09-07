PROG1 = cicada.exe
CICADA_SRCS1 := cicada.cc transaction.cc util.cc

REL := ../common/
include $(REL)Makefile
CICADA_ALLSRC = $(CICADA_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DADD_ANALYSIS=1 \
				 -DINLINE_VERSION_PROMOTION=1 \
				 -DINLINE_VERSION_OPT=1 \
				 -DMASSTREE_USE=1 \
				 -DPARTITION_TABLE=0 \
				 -DREUSE_VERSION=1 \
				 -DSINGLE_EXEC=0 \
				 -DUSE_BACKOFF=1 \
				 -DWRITE_LATEST_ONLY=0 \

# WRITE_LATEST_ONLY : for all write. 
# original model adapt write-latest-only-rule for RMW
# , not for blind write.

# SINGLE_EXEC option can be executed 
# only non conflict workload.
# It must use PARTION_TABLE for dividing 
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
