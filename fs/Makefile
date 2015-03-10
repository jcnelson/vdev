CPP    := g++ -Wall -g -fstack-protector -fstack-protector-all -Wno-unused-but-set-variable -Wno-unused-variable
LIB   := -lpthread -lrt -lpstat -lvdev -lfskit -lfskit_fuse -lfuse
LIBINC := -L../libvdev
INC   := -I. -I.. -I/usr/include -I/usr/local/include
C_SRCS:= $(wildcard *.c) 
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

VDEVFS := vdevfs

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/sbin

all: $(VDEVFS)

$(VDEVFS): $(OBJ)
	$(CPP) -o $(VDEVFS) $(OBJ) $(LIBINC) $(LIB)

install: $(VDEVFS)
	mkdir -p $(BINDIR)
	cp -a $(VDEVFS) $(BINDIR)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o: %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(VDEVFS)
