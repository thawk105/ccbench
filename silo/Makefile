PROG1 = silo.exe
SILO_SRCS1 := silo.cc transaction.cc util.cc
PROG2 = replayTest.exe
SRCS2 := replayTest.cc

REL := ../common/
include $(REL)Makefile
SILO_ALLSRC = $(SILO_SRCS1) $(SRCS2) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DADD_ANALYSIS=1 \
				 -DMASSTREE_USE=1 \
				 -DPARTITION_TABLE=0 \
				 -DPROCEDURE_SORT=0 \

LDFLAGS = -L../third_party/mimalloc/out/release -L../third_party/mimalloc/out/debug
LIBS = -lpthread #-lmimalloc

OBJS1 = $(SILO_SRCS1:.cc=.o)
OBJS2 = $(SRCS2:.cc=.o)

all: $(PROG1) $(PROG2)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PROG2) : $(OBJS2) 
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(SILO_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
	rm -rf .deps
