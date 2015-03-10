CPP    := gcc -Wall -g -std=c99
LIB   := -lpthread -lrt 
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c) $(wildcard os/*.c)
CXSRCS:= $(wildcard *.cpp) $(wildcard os/*.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -fstack-protector -fstack-protector-all

HELPERS := stat_optical stat_ata stat_input stat_scsi stat_v4l stat_net stat_usb stat_bus stat_path
HELPER_SCRIPTS := $(wildcard *.sh)

PREFIX ?= /
BINDIR ?= $(PREFIX)/lib/vdev/

DEBUG ?= 0

all: $(HELPERS)

stat_%: stat_%.o common.o
	$(CPP) -o $@ $(INC) $(LIBINC) $(LIB) $< common.o

install: $(HELPERS)
	mkdir -p $(BINDIR)
	cp -a $(HELPERS) $(HELPER_SCRIPTS) $(BINDIR)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS) -DDEBUG=$(DEBUG)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS) -DDEBUG=$(DEBUG)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(HELPERS)
