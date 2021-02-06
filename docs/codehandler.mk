PREFIX 	:= powerpc-eabi
GCC 	:= $(PREFIX)-gcc
OBJCOPY := $(PREFIX)-objcopy

CFLAGS 	:= -nostartfiles -nodefaultlibs -Wl,-Ttext,0x80001800

all:
	$(GCC) $(CFLAGS) -o codehandler.elf codehandler.s
	$(OBJCOPY) -O binary codehandler.elf codehandler.bin
clean:
	rm -v codehandler.elf codehandler.bin

