include ../buildconf.mk

HWDB_INPUT := $(wildcard udev/*.hwdb)
HWDB_GEN := gen_database.sh
HWDB_BUILD_INPUT := $(patsubst udev/%.hwdb,$(BUILD_HWDB)/input/%.hwdb,$(HWDB_INPUT))
HWDB_BUILD_GEN := $(patsubst %.sh,$(BUILD_HWDB)/%.sh,$(HWDB_GEN))
HWDB_BUILD := $(BUILD_HWDB)/hwdb.squashfs

HWDB_INSTALL := $(INSTALL_HWDB)/hwdb.squashfs 

all: $(HWDB_BUILD)

$(HWDB_BUILD): $(HWDB_BUILD_INPUT) $(HWDB_BUILD_GEN)
	@echo "Generating hardware database"
	@mkdir -p "$@.dir"
	$(BUILD_HWDB)/gen_database.sh -o "$@.dir" $(BUILD_HWDB)/input/*.hwdb
	mksquashfs -force-uid 0 -force-gid 0 "$@.dir" "$@"

$(BUILD_HWDB)/input/%.hwdb: udev/%.hwdb
	@mkdir -p "$(shell dirname "$@")"
	@cp -a "$<" "$@"

$(BUILD_HWDB)/%.sh: %.sh
	@mkdir -p "$(shell dirname "$@")"
	@cp -a "$<" "$@"

install: $(HWDB_INSTALL)

$(INSTALL_HWDB)/%: $(BUILD_HWDB)/%
	mkdir -p "$(shell dirname "$@")"
	cp -a "$<" "$@"

.PHONY: uninstall
uninstall:
	rm -f "$(HWDB_INSTALL)"

.PHONY: clean 
clean:
	rm -rf $(HWDB_BUILD) $(HWDB_BUILD_INPUT) $(HWDB_BUILD_GEN) $(HWDB_BUILD).dir

