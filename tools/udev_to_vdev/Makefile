CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -Wall -g
CXXFLAGS ?= -Wall -g -std=c++11
LIB   := 
INC   := -I/usr/include -I/usr/local/include -I. 
C_SRCS:= $(wildcard *.c)
CXX_SRCS:= $(wildcard *.cpp)
OBJ   := $(patsubst %.c,%.o,$(C_SRCS)) $(patsubst %.cpp,%.o,$(CXSRCS))
DEFS  := -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS

UDEV_TO_VDEV := udev2vdev

DESTDIR := /
BIN_DIR := /usr/local/bin

all: udev2vdev

udev2vdev: $(OBJ)
	$(CXX) -o $(UDEV_TO_VDEV) $(OBJ) $(LIBINC) $(LIB)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ $(INC) -c $< $(DEFS)

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(INC) -c $< $(DEFS)

.PHONY: clean
clean:
	/bin/rm -f $(OBJ) $(UDEV_TO_VDEV)
