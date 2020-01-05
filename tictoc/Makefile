PROG1 = tictoc.exe
TICTOC_SRCS1 := tictoc.cc transaction.cc util.cc

REL := ../common/
include $(REL)Makefile
TICTOC_ALLSRC = $(TICTOC_SRCS1) $(wildcard include/*.hh)

# start of initialization of some parameters
KEY_SIZE=8
VAL_SIZE=4
ADD_ANALYSIS=1
BACK_OFF=0
MASSTREE_USE=1
NO_WAIT_LOCKING_IN_VALIDATION=1
PREEMPTIVE_ABORTS=1
TIMESTAMP_HISTORY=1

SLEEP_READ_PHASE=0 # # of tics
#end of initialization

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) \
				 -DADD_ANALYSIS=$(ADD_ANALYSIS) \
				 -DBACK_OFF=$(BACK_OFF) \
				 -DMASSTREE_USE=$(MASSTREE_USE) \
				 -DNO_WAIT_LOCKING_IN_VALIDATION=$(NO_WAIT_LOCKING_IN_VALIDATION) \
				 -DPREEMPTIVE_ABORTS=$(PREEMPTIVE_ABORTS) \
				 -DTIMESTAMP_HISTORY=$(TIMESTAMP_HISTORY) \
				 -DSLEEP_READ_PHASE=$(SLEEP_READ_PHASE)

LDFLAGS = -L../third_party/mimalloc/out/release
LIBS = -lpthread -lmimalloc
#LIBS = -lpthread

OBJS1 = $(TICTOC_SRCS1:.cc=.o)
all: $(PROG1)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(TICTOC_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
	rm -rf .deps
