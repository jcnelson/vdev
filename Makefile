CC=g++ -Wall 
LD=-lfuse -lpthread -lrt
OUT=loginfs
SRC=main.cpp loginfs.cpp entry.cpp

all: loginfs

loginfs:
	$(CC) -o $(OUT) $(LD)

*.cpp:
	$(CC) -o $
