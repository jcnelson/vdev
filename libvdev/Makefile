CPP    := gcc -Wall -g -fPIC -std=c99 -Wno-unused-but-set-variable -Wno-unused-variable
LIB   := 
INC   := -I. -I/usr/include -I/usr/local/include 
C_SRCS:= $(wildcard *.c)

HEADERS := $(wildcard *.h)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS))

DEFS  := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_BSD_SOURCE

VERSION_MAJOR := 1
VERSION_MINOR := 0
VERSION_PATCH := 1

LIBVDEV := libvdev.so
LIBVDEV_SO := libvdev.so.$(VERSION_MAJOR)
LIBVDEV_LIB := libvdev.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

PREFIX ?= /
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include/libvdev

all: libvdev 

libvdev: $(OBJ)
	$(CPP) -shared -Wl,-soname,$(LIBVDEV_SO) -o $(LIBVDEV_LIB) $(OBJ) $(LIBINC) $(LIB)
	$(SHELL) -c "if ! test -L $(LIBVDEV_SO); then /bin/ln -s $(LIBVDEV_LIB) $(LIBVDEV_SO); fi"
	$(SHELL) -c "if ! test -L $(LIBVDEV); then /bin/ln -s $(LIBVDEV_SO) $(LIBVDEV); fi"

libvdev-install: libvdev
	mkdir -p $(LIBDIR))
	cp -a $(LIBVDEV) $(LIBVDEV_SO) $(LIBVDEV_LIB) $(LIBDIR)

libvdev-dev-install: libvdev
	mkdir -p $(INCLUDEDIR) 
	cp -a $(wildcard *.h) $(INCLUDEDIR)

install: libvdev-install libvdev-dev-install

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(LIBVDEV_LIB) $(LIBVDEV_SO) $(LIBVDEV)
