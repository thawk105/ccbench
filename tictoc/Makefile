PROG1 = tictoc.exe
TICTOC_SRCS1 := tictoc.cc transaction.cc util.cc

REL := ../common/
include $(REL)Makefile
TICTOC_ALLSRC = $(TICTOC_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4

CC = g++
CFLAGS = -c -pipe -g -O2 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) \
				 -DNO_WAIT_LOCKING_IN_VALIDATION=1 \
				 -DPREEMPTIVE_ABORTS=1 \
				 -DTIMESTAMP_HISTORY=1 \
				 -DMASSTREE_USE=1 \
				 -DADD_ANALYSIS=1 \

LDFLAGS = -L../third_party/mimalloc/out/release
LIBS = -lpthread -lmimalloc

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
