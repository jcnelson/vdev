CPP    := cc -Wall -g
LIB   := -lpthread -lrt 
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c) $(wildcard os/*.c)
CXSRCS:= $(wildcard *.cpp) $(wildcard os/*.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -fstack-protector -fstack-protector-all

HELPERS := stat_optical stat_ata
HELPER_SCRIPTS := $(wildcard *.sh)

PREFIX ?= /
BINDIR ?= $(PREFIX)/lib/vdev/

all: $(HELPERS)

stat_%: stat_%.o
	$(CPP) -o $@ $(INC) $(LIBINC) $(LIB) $<

install: $(HELPERS)
	mkdir -p $(BINDIR)
	cp -a $(HELPERS) $(HELPER_SCRIPTS) $(BINDIR)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(HELPERS)
