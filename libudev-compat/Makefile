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

LIBUDEV := libudev.so
LIBUDEV_SO := libudev.so.$(VERSION_MAJOR)
LIBUDEV_LIB := libudev.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

PREFIX ?= /
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include/libudev
PKGCONFIGDIR ?= $(PREFIX)/lib/pkgconfig

all: libudev-compat

libvdev-compat: $(OBJ)
	$(CPP) -shared -Wl,-soname,$(LIBUDEV_SO) -o $(LIBUDEV_LIB) $(OBJ) $(LIBINC) $(LIB)
	$(SHELL) -c "if ! test -L $(LIBUDEV_SO); then /bin/ln -s $(LIBUDEV_LIB) $(LIBUDEV_SO); fi"
	$(SHELL) -c "if ! test -L $(LIBUDEV); then /bin/ln -s $(LIBUDEV_SO) $(LIBUDEV); fi"

libudev-install: libudev-compat
	mkdir -p $(LIBDIR)
	cp -a $(LIBUDEV) $(LIBUDEV_SO) $(LIBUDEV_LIB) $(LIBDIR)

libudev-dev-install: libudev-compat
	mkdir -p $(INCLUDEDIR) 
	cp -a $(wildcard *.h) $(INCLUDEDIR)

install: libudev-install libudev-dev-install

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(LIBUDEV_LIB) $(LIBUDEV_SO) $(LIBUDEV)
