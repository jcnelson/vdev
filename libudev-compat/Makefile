include ../buildconf.mk

CFLAGS := -Wall -g -fPIC -fstack-protector -fstack-protector-all -pthread -Wno-unused-variable -Wno-unused-but-set-variable
INC := $(INC) -I$(BUILD_LIBUDEV_COMPAT_HEADERS)
DEFS := $(DEFS) -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=0700 -D_BSD_SOURCE -D_GNU_SOURCE
CC   := gcc $(CFLAGS) $(INC) $(DEFS)

C_SRCS:= $(wildcard *.c)

HEADERS := $(wildcard *.h)
OBJ := $(patsubst %.c,$(BUILD_LIBUDEV_COMPAT)/%.o,$(C_SRCS))
OBJ_TEST := $(filter-out $(BUILD_LIBUDEV_COMPAT)/libudev-fs.o,$(OBJ))
LIB :=

VERSION_MAJOR := 1
VERSION_MINOR := 5
VERSION_PATCH := 1

LIBUDEV_COMPAT := $(BUILD_LIBUDEV_COMPAT)/libudev.so
LIBUDEV_COMPAT_SONAME := libudev.so.$(VERSION_MAJOR)
LIBUDEV_COMPAT_SO := $(BUILD_LIBUDEV_COMPAT)/libudev.so.$(VERSION_MAJOR)
LIBUDEV_COMPAT_LIB := $(BUILD_LIBUDEV_COMPAT)/libudev.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
 
LIBUDEV_COMPAT_FILES := $(LIBUDEV_COMPAT) $(LIBUDEV_COMPAT_SO) $(LIBUDEV_COMPAT_LIB) $(LIBUDEV_COMPAT_A)

LIBUDEV_COMPAT_HEADERS := $(patsubst %.h,$(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h,$(HEADERS))

LIBUDEV_COMPAT_FILES_INSTALL := $(patsubst $(BUILD_LIBUDEV_COMPAT)/%,$(LIBDIR)/%,$(LIBUDEV_COMPAT_FILES))

LIBUDEV_COMPAT_HEADERS_INSTALL := $(patsubst $(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h,$(INCLUDEDIR)/libudev/%.h,$(LIBUDEV_COMPAT_HEADERS))

all: $(LIBUDEV_COMPAT_FILES) $(LIBUDEV_COMPAT_HEADERS)

test: $(BUILD_LIBUDEV_COMPAT)/libudev-fs 

$(LIBUDEV_COMPAT): $(OBJ)
	@mkdir -p $(shell dirname "$@")
	$(CC) -shared -Wl,-soname,$(LIBUDEV_COMPAT_SONAME) -o $@ $(OBJ) $(LIBINC) $(LIB)

$(LIBUDEV_COMPAT_LIB): $(LIBUDEV_COMPAT)
	@mkdir -p "$(shell dirname "$@")"
	@ln -sf "$(shell basename "$<")" "$@"

$(LIBUDEV_COMPAT_SO): $(LIBUDEV_COMPAT_LIB)
	@mkdir -p "$(shell dirname "$@")"
	@ln -sf "$(shell basename $<)" "$@"

$(BUILD_LIBUDEV_COMPAT)/libudev-fs: $(BUILD_LIBUDEV_COMPAT)/libudev-fs-test.o $(OBJ_TEST)
	$(CC) -o "$@" "$<" $(OBJ_TEST) $(LIB) $(LIBINC) 

$(BUILD_LIBUDEV_COMPAT)/libudev-fs-test.o: libudev-fs.c $(LIBUDEV_COMPAT_HEADERS)
	$(CC) -o "$@" -c "$<" -DTEST

$(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h: %.h
	@mkdir -p $(shell dirname "$@")
	@cp -a "$<" "$@"

$(BUILD_LIBUDEV_COMPAT)/%.o: %.c $(LIBUDEV_COMPAT_HEADERS)
	@mkdir -p $(shell dirname "$@")
	$(CC) -o "$@" -c "$<"

install: $(LIBUDEV_COMPAT_FILES_INSTALL) $(LIBUDEV_COMPAT_HEADERS_INSTALL)

$(LIBDIR)/%: $(BUILD_LIBUDEV_COMPAT)/%
	@mkdir -p $(shell dirname "$@")
	@cp -a "$<" "$@"

$(INCLUDEDIR)/libudev/%.h: $(BUILD_LIBUDEV_COMPAT_HEADERS)/%.h
	@mkdir -p $(shell dirname "$@")
	@cp -a "$<" "$@"

.PHONY: uninstall
uninstall:
	rm -f $(LIBUDEV_COMPAT_FILES_INSTALL) $(LIBUDEV_COMPAT_HEADERS_INSTALL)

.PHONY: clean 
clean:
	rm -f $(LIBUDEV_COMPAT_FILES) $(LIBUDEV_COMPAT_HEADERS) $(OBJ)
