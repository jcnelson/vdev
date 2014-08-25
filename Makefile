CPP    := g++ -Wall -g
LIB   := -lfuse -lpthread -lrt
INC   := -I/usr/include -I.
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

LOGINFS := loginfs
LIBANCELLARY_DIR := libancellary/

DESTDIR = /
BIN_DIR = /usr/bin

all: loginfs

loginfs: $(OBJ) libancillary
	$(CPP) -o $(LOGINFS) $(OBJ) $(LIBINC) $(LIB)

libancillary:
	$(MAKE) -C $(LIBANCELLARY_DIR)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(LOGINFS)
	$(MAKE) -C $(LIBANCELLARY_DIR) clean
