AR = ar
CC = gcc
CXX = g++
CPPFLAGS = 
CXXFLAGS = -g -W -Wall -O2 -pipe
CXXFLAGS += -march=native

DEPSDIR := .deps
DEPCFLAGS = #-MD -MF $(DEPSDIR)/$*.d -MP
ifeq ($(strip $(MEMMGR)), )
  MEMMGR = 
endif
ifneq ($(strip $(KEYSWAP)), )
  CPPFLAGS += -DKEYSWAP
endif
ifneq ($(strip $(NOPREFETCH)), )
  CPPFLAGS += -DNOPREFETCH
endif
ifneq ($(strip $(NOSUPERPAGE)), )
  CPPFLAGS += -DNOSUPERPAGE
endif

LIBS = -lnuma  -lpthread -lm
ifeq (0, 0) # for easy switch
ifeq ($(shell hostname), dbs11)
LIBS += -ltbb -ltbbmalloc_proxy -ltbbmalloc
endif
endif

DEFINE = 
ifeq ($(shell hostname), dbs11)
DEFINE += -Ddbs11
endif

INCLUDE =
ifeq ($(shell hostname), dbs11)
INCLUDE += -I/home/tanabe/package/tbb/include/tbb
endif

LDFLAGS = 
ifeq ($(shell hostname), dbs11)
LDFLAGS += -L/home/tanabe/package/tbb/build/linux_intel64_gcc_cc7_libc2.27_kernel4.15.0_release
endif

all: unit-mt unit-mt2

%.o: %.c ../../masstree/config.h #$(DEPSDIR)/stamp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(INCLUDE) $(DEFINE) -c -o $@ $<

%.o: %.cc ../../masstree/config.h #$(DEPSDIR)/stamp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(INCLUDE) $(DEFINE) -c -o $@ $<

%.S: %.o
	objdump -S $< > $@

../../masstree/libjson.a: ../../masstree/json.o ../../masstree/string.o ../../masstree/straccum.o ../../masstree/str.o ../../masstree/msgpack.o \
	../../masstree/clp.o ../../masstree/kvrandom.o ../../masstree/compiler.o ../../masstree/memdebug.o ../../masstree/kvthread.o
	@/bin/rm -f $@
	$(AR) cru $@ $^

unit-mt: unit-mt.o ../../masstree/compiler.o ../../masstree/misc.o ../../masstree/libjson.a ../../common/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(MEMMGR) $(LDFLAGS) $(LIBS)

unit-mt2: unit-mt2.o ../../masstree/compiler.o ../../masstree/misc.o ../../masstree/libjson.a ../../common/util.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(MEMMGR) $(LDFLAGS) $(LIBS)

$(DEPSDIR)/stamp:
	mkdir -p $(DEPSDIR)
	touch $@

clean:
	rm -f mtd mtclient mttest test_string test_atomics *.o libjson.a
	rm -rf .deps
	rm -f ../../common/*.o

DEPFILES := $(wildcard $(DEPSDIR)/*.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

.PHONY: clean all
