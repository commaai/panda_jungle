CFLAGS += -I inc -I ../ -nostdlib -fno-builtin -std=gnu11 -Os

CFLAGS += -Tstm32_flash.ld

DFU_UTIL = "dfu-util"

# Only DEV builds for the jungle
BUILDER = DEV

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump

ifeq ($(RELEASE),1)
  CERT = ../../pandaextra/certs/release
else
  # enable the debug cert
  CERT = ../certs/debug
  CFLAGS += "-DALLOW_DEBUG"
endif

ifeq ($(TESTING_CLOSET),1)
	CFLAGS += "-DTESTING_CLOSET"
endif

DEPDIR = generated_dependencies
$(shell mkdir -p -m 777 $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

# this no longer pushes the bootstub
flash: obj/$(PROJ_NAME).bin
	PYTHONPATH=../ python -c "from python import PandaJungle; PandaJungle().flash('obj/$(PROJ_NAME).bin')"

bin: obj/$(PROJ_NAME).bin

# this flashes everything
recover: obj/bootstub.$(PROJ_NAME).bin obj/$(PROJ_NAME).bin
	-PYTHONPATH=../ python -c "from python import PandaJungle; PandaJungle().reset(enter_bootloader=True)"
	sleep 1.0
	$(DFU_UTIL) -d 0483:df11 -a 0 -s 0x08004000 -D obj/$(PROJ_NAME).bin
	$(DFU_UTIL) -d 0483:df11 -a 0 -s 0x08000000:leave -D obj/bootstub.$(PROJ_NAME).bin

include ../common/version.mk

obj/cert.h: ../crypto/getcertheader.py
	../crypto/getcertheader.py ../certs/debug.pub ../certs/release.pub > $@

obj/%.$(PROJ_NAME).o: %.c obj/gitversion.h obj/cert.h $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(CFLAGS) -o $@ -c $<
	$(POSTCOMPILE)

obj/%.$(PROJ_NAME).o: ../crypto/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

obj/$(STARTUP_FILE).o: $(STARTUP_FILE).s
	$(CC) $(CFLAGS) -o $@ -c $<

obj/$(PROJ_NAME).bin: obj/$(STARTUP_FILE).o obj/main.$(PROJ_NAME).o
  # hack
	$(CC) -Wl,--section-start,.isr_vector=0x8004000 $(CFLAGS) -o obj/$(PROJ_NAME).elf $^
	$(OBJCOPY) -v -O binary obj/$(PROJ_NAME).elf obj/code.bin
	SETLEN=1 ../crypto/sign.py obj/code.bin $@ $(CERT)
	@BINSIZE=$$(du -b "obj/$(PROJ_NAME).bin" | cut -f 1) ; \
	if [ $$BINSIZE -ge 49152 ]; then echo "ERROR obj/$(PROJ_NAME).bin is too big!"; exit 1; fi;

obj/bootstub.$(PROJ_NAME).bin: obj/$(STARTUP_FILE).o obj/bootstub.$(PROJ_NAME).o obj/sha.$(PROJ_NAME).o obj/rsa.$(PROJ_NAME).o
	$(CC) $(CFLAGS) -o obj/bootstub.$(PROJ_NAME).elf $^
	$(OBJCOPY) -v -O binary obj/bootstub.$(PROJ_NAME).elf $@

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(wildcard *.c))))

clean:
	@$(RM) obj/*
	@rm -rf $(DEPDIR)
