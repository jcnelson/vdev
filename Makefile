CPP    := g++ -Wall -g
LIB   := -lfuse -lpthread -lrt -lfskit
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

VDEV := vdev

DESTDIR = /
BIN_DIR = /usr/local/bin

all: vdev

vdev: $(OBJ)
	$(CPP) -o $(VDEV) $(OBJ) $(LIBINC) $(LIB)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(VDEV)
