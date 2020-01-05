PROG1 = ss2pl.exe
SS2PL_SRCS1 := ss2pl.cc transaction.cc util.cc

REL := ../common/
include $(REL)Makefile
SS2PL_ALLSRC = $(SS2PL_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=4
ADD_ANALYSIS=1
BACK_OFF=0
KEY_SORT=1
MASSTREE_USE=1

CC = g++
CFLAGS = -c -pipe -g -O3 -std=c++17 -march=native \
         -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
         -D$(shell uname) \
         -D$(shell hostname) \
         -DDLR1 \
         -DKEY_SIZE=$(KEY_SIZE) \
				 -DVAL_SIZE=$(VAL_SIZE) \
         -DADD_ANALYSIS=$(ADD_ANALYSIS) \
         -DBACK_OFF=$(BACK_OFF) \
         -DKEY_SORT=$(KEY_SORT) \
         -DMASSTREE_USE=$(MASSTREE_USE) \

# dead lock resolution.
# DLR0 ... timeout
# DLR1 ... no-wait

LDFLAGS = -L../third_party/mimalloc/out/release
LIBS = -lpthread -lmimalloc

OBJS1 = $(SS2PL_SRCS1:.cc=.o)
all: $(PROG1)

include ../include/MakefileForMasstreeUse
$(PROG1) : $(OBJS1) $(MASSOBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

format:
	clang-format -i -verbose -style=Google $(SS2PL_ALLSRC)

clean:
	rm -f *~ *.o *.exe *.stackdump
	rm -f ../common/*~ ../common/*.o ../common/*.exe ../common/*.stackdump
	rm -rf .deps
