PROG1 = ermia.exe
ERMIA_SRCS1 = ermia.cc transaction.cc util.cc garbage_collection.cc

REL := ../common/
include $(REL)Makefile
ERMIA_ALLSRC = $(ERMIA_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
				 -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
				 -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) \
				 -D$(shell uname) \
				 -D$(shell hostname) \
				 -DMASSTREE_USE=1 \
				 -DADD_ANALYSIS=1 \

LDFLAGS = -L../third_party/mimalloc/out/release
LIBS = -lpthread -lmimalloc

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


