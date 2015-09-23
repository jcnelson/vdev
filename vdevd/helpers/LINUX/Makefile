include ../../../buildconf.mk

C_SRCS:= $(wildcard *.c) 
CXSRCS:= $(wildcard *.cpp) 
OBJ   := $(patsubst %.c,$(BUILD_VDEVD_HELPERS)/%.o,$(C_SRCS)) $(patsubst %.cpp,$(BUILD_VDEVD_HELPERS)/%.o,$(CXSRCS))
LIB   := -lrt

HELPER_SCRIPTS := $(wildcard *.sh) daemonlet
HELPERS := stat_optical stat_ata stat_input stat_scsi stat_v4l stat_net stat_usb stat_bus stat_path event-put echo_n $(HELPER_SCRIPTS)

HELPERS_BUILD := $(patsubst %,$(BUILD_VDEVD_HELPERS)/%,$(HELPERS))
HELPERS_INSTALL := $(patsubst %,$(INSTALL_VDEVD_HELPERS)/%,$(HELPERS))

all: $(HELPERS_BUILD)

$(BUILD_VDEVD_HELPERS)/%: $(BUILD_VDEVD_HELPERS)/%.o $(BUILD_VDEVD_HELPERS)/common.o
	@mkdir -p "$(shell dirname "$@")"
	$(CC) -o "$@" "$<" $(BUILD_VDEVD_HELPERS)/common.o $(LIBINC) $(LIB) $(LDFLAGS)

$(BUILD_VDEVD_HELPERS)/%.o: %.c
	@mkdir -p "$(shell dirname "$@")"
	$(CC) $(CFLAGS) $(DEFS) $(INC) -o "$@" -c "$<"

$(BUILD_VDEVD_HELPERS)/%.sh: %.sh
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

$(BUILD_VDEVD_HELPERS)/daemonlet: daemonlet
	@mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

install: $(HELPERS_INSTALL)
$(INSTALL_VDEVD_HELPERS)/%: $(BUILD_VDEVD_HELPERS)/%
	@mkdir -p "$(shell dirname "$@")"
	@rm -f "$@"
	cp -a "$<" "$@"

.PHONY: uninstall
uninstall:
	rm -f $(HELPERS_INSTALL)

.PHONY: clean
clean:
	rm -f $(OBJ) $(HELPERS_BUILD)
