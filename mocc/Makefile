PROG1 = mocc.exe
MOCC_SRCS1 := mocc.cc transaction.cc lock.cc util.cc

REL := ../common/
include $(REL)Makefile
MOCC_ALLSRC = $(MOCC_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4
ADD_ANALYSIS=1
BACK_OFF=0
KEY_SORT=1
MASSTREE_USE=1
TEMPERATURE_RESET_OPT=1

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -D$(shell uname) \
				 -D$(shell hostname) \
			 	 -DRWLOCK \
				 -DKEY_SIZE=$(KEY_SIZE) \
				 -DVAL_SIZE=$(VAL_SIZE) \
				 -DADD_ANALYSIS=$(ADD_ANALYSIS) \
				 -DBACK_OFF=$(BACK_OFF) \
				 -DKEY_SORT=$(KEY_SORT) \
				 -DMASSTREE_USE=$(MASSTREE_USE) \
				 -DTEMPERATURE_RESET_OPT=$(TEMPERATURE_RESET_OPT) \

LDFLAGS = -L../third_party/mimalloc/out/release
LIBS = -lpthread -lmimalloc

ifneq ($(shell hostname), chris41.omni.hpcc.jp)
ifneq ($(shell hostname), dbs11)
LIBS = -lpthread
endif
endif

OBJS1 = $(MOCC_SRCS1:.cc=.o)

all : $(PROG1)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(MOCC_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump *.log
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
