CPP    := g++ -Wall -g -fstack-protector -fstack-protector-all
LIB   := -lpthread -lrt -lpstat
LIB_FS := -lfskit -lfskit_fuse -lfuse
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c) $(wildcard os/*.c)
CXSRCS:= $(wildcard *.cpp) $(wildcard os/*.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

VDEV := vdev

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

# change/override this for your OS
OS := LINUX

# optionally compile in the filesystem 
ifdef USE_FS
	LIB += $(LIB_FS)
	DEFS += -D_USE_FS
endif

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

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS) -D_VDEV_OS_$(OS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(VDEV)
	$(MAKE) -C $(HELPERS_DIR) clean
