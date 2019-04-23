AR = ar
CC = gcc
CXX = g++
CPPFLAGS = 
CXXFLAGS = -g -W -Wall -O3
DEPSDIR := .deps
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP
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
LDFLAGS = 

all: unit-mt

%.o: %.c config.h $(DEPSDIR)/stamp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) -include config.h -c -o $@ $<

%.o: %.cc config.h $(DEPSDIR)/stamp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) -include config.h -c -o $@ $<

%.S: %.o
	objdump -S $< > $@

libjson.a: json.o string.o straccum.o str.o msgpack.o \
	clp.o kvrandom.o compiler.o memdebug.o kvthread.o
	@/bin/rm -f $@
	$(AR) cru $@ $^

unit-mt: unit-mt.o compiler.o misc.o libjson.a
	$(CXX) $(CXXFLAGS) -o $@ $^ $(MEMMGR) $(LDFLAGS) $(LIBS)

$(DEPSDIR)/stamp:
	mkdir -p $(DEPSDIR)
	touch $@

clean:
	rm -f mtd mtclient mttest test_string test_atomics *.o libjson.a
	rm -rf .deps

DEPFILES := $(wildcard $(DEPSDIR)/*.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

.PHONY: clean all
