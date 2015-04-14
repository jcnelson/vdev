all:
	$(MAKE) -C libvdev
	$(MAKE) -C vdevd
	$(MAKE) -C fs
	$(MAKE) -C example

install:
	$(MAKE) -C libvdev install
	$(MAKE) -C vdevd install
	$(MAKE) -C fs install
	$(MAKE) -C example install

initramfs:
	$(MAKE) -C example initramfs
	@echo ""
	@echo "Initramfs image generated at example/initrd.img-`uname -r`."
	@echo "You must now configure your bootloader to use this initramfs image"
	@echo "if you want to boot with vdevd."
	@echo ""

clean:
	$(MAKE) -C libvdev clean
	$(MAKE) -C vdevd clean
	$(MAKE) -C fs clean
