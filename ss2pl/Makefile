PROG1 = ss2pl.exe
SS2PL_SRCS1 := ss2pl.cc transaction.cc util.cc

REL := ../common/
include $(REL)Makefile
SS2PL_ALLSRC = $(SS2PL_SRCS1) $(wildcard include/*.hh)

KEY_SIZE=8
VAL_SIZE=1000

CC = g++
CFLAGS = -c -pipe -g -O2 -std=c++17 -march=native \
         -Wall -Wextra -Wdangling-else -Wchkp -Winvalid-memory-model \
         -D$(shell uname) \
         -D$(shell hostname) \
         -DKEY_SIZE=$(KEY_SIZE) -DVAL_SIZE=$(VAL_SIZE) \
         -DMASSTREE_USE=1 \
         -DKEY_SORT=0 \
         -DDLR1 \
         -DADD_ANALYSIS=1 \

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
