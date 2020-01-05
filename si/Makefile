PROG1 = si.exe
SI_SRCS1 = si.cc transaction.cc util.cc garbage_collection.cc

REL := ../common/
include $(REL)Makefile
SI_ALLSRC = $(SI_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=10000
ADD_ANALYSIS=1
BACK_OFF=0
MASSTREE_USE=1

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -DTBB_DO_THREADING_TOOLS=1 \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DCCTR_ON \
				 -DKEY_SIZE=$(KEY_SIZE) \
				 -DVAL_SIZE=$(VAL_SIZE) \
				 -DADD_ANALYSIS=$(ADD_ANALYSIS) \
				 -DBACK_OFF=$(BACK_OFF) \
				 -DMASSTREE_USE=$(MASSTREE_USE) \

LDFLAGS = -L../third_party/mimalloc/out/release
LIBS = -lpthread -lmimalloc

OBJS1 = $(SI_SRCS1:.cc=.o)

all: $(PROG1)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(SI_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stachdump
	rm -rf .deps

