PROG1 = silo.exe
SILO_SRCS1 := silo.cc transaction.cc util.cc result.cc
PROG2 = replayTest.exe
SRCS2 := replayTest.cc

REL := ../common/
include $(REL)Makefile
SILO_ALLSRC = $(SILO_SRCS1) $(SRCS2) $(wildcard include/*.hh)

# start of initialization of some parameters.
ADD_ANALYSIS=1
BACK_OFF=0
KEY_SIZE=8
MASSTREE_USE=1
NO_WAIT_LOCKING_IN_VALIDATION=0
NO_WAIT_OF_TICTOC=0
PARTITION_TABLE=0
PROCEDURE_SORT=0
SLEEP_READ_PHASE=0 # of tics
VAL_SIZE=4
WAL=0
# end of initialization

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DKEY_SIZE=$(KEY_SIZE) \
				 -DVAL_SIZE=$(VAL_SIZE) \
				 -DADD_ANALYSIS=$(ADD_ANALYSIS) \
				 -DBACK_OFF=$(BACK_OFF) \
				 -DMASSTREE_USE=$(MASSTREE_USE) \
				 -DNO_WAIT_LOCKING_IN_VALIDATION=$(NO_WAIT_LOCKING_IN_VALIDATION) \
				 -DNO_WAIT_OF_TICTOC=$(NO_WAIT_OF_TICTOC) \
				 -DPARTITION_TABLE=$(PARTITION_TABLE) \
				 -DPROCEDURE_SORT=$(PROCEDURE_SORT) \
				 -DSLEEP_READ_PHASE=$(SLEEP_READ_PHASE) \
				 -DWAL=$(WAL) \

INCLUDE = -I/usr/include \
					-I../third_party/ \

LDLIBS = -lpthread -lboost_filesystem -lboost_system -lgflags -lglog

OBJS1 = $(SILO_SRCS1:.cc=.o)
OBJS2 = $(SRCS2:.cc=.o)

all: $(PROG1) $(PROG2)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS) $(INCLUDE)

$(PROG2) : $(OBJS2) 
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS) $(INCLUDE)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(SILO_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
	rm -rf .deps
