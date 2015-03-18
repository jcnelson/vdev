CPP    := gcc -std=c99 -Wall -g -fstack-protector -fstack-protector-all -Wno-unused-but-set-variable -Wno-unused-variable
LIB   := -lpthread -lrt -lvdev
INC   := -I. -I../ -I/usr/include -I/usr/local/include
LIBINC := -L../libvdev
C_SRCS:= $(wildcard *.c) $(wildcard os/*.c)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) 
DEFS  := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

VDEV := vdevd

PREFIX ?= /
BINDIR ?= $(PREFIX)/sbin

# change/override this for your OS
OS := LINUX

HELPERS_DIR := ./helpers/$(OS)

all: $(VDEV) helpers

$(VDEV): $(OBJ) helpers
	$(CPP) -o $(VDEV) $(OBJ) $(LIBINC) $(LIB) -D_VDEV_OS_$(OS)

.PHONY: helpers
helpers:
	$(MAKE) -C $(HELPERS_DIR)

install: $(VDEV)
	mkdir -p $(BINDIR)
	cp -a $(VDEV) $(BINDIR)
	$(MAKE) -C $(HELPERS_DIR) install

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS) -D_VDEV_OS_$(OS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(VDEV)
	$(MAKE) -C $(HELPERS_DIR) clean
