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

# SINGLE_EXEC option can be executed only non conflict workload.
# スレッドごとにアクセスするブロックをパーティションで分割し，同一キーに対しては
# write -> read の順序を維持する．
#
# SINGLE_EXEC の時は，INLINE_VERSION_OPT をオンにする．そうでないとバグるかもしれない．
# すなわちレコードヘッダが latest pointer を持っていて，その先にシングルバージョンがあるケースは想定していない．
# レコードヘッダにインラインバージョンさせた方が早そうなので，そうで無いケースは実装していない．

LDFLAGS = -L../third_party/mimalloc/out/release -L../third_party/mimalloc/out/debug
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
