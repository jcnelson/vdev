CPP    := g++ -Wall -g -std=c++11
LIB   := 
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c)
CXSRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS

UDEV_TO_VDEV := udev2vdev

DESTDIR := /
BIN_DIR := /usr/local/bin

all: udev2vdev

udev2vdev: $(OBJ)
	$(CPP) -o $(UDEV_TO_VDEV) $(OBJ) $(LIBINC) $(LIB)

%.o : %.c
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CPP) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(UDEV_TO_VDEV)
