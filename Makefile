# Globals and files to compile.
KERNEL    := qword
KERNELBIN := $(KERNEL).bin
KERNELELF := $(KERNEL).elf

LAI_URL := https://github.com/qword-os/lai.git
LAI_DIR := acpi/lai

CFILES    := $(shell find . -type f -name '*.c')
ASMFILES  := $(shell find . -type f -name '*.asm')
REALFILES := $(shell find . -type f -name '*.real')
BINS      := $(REALFILES:.real=.bin)
OBJ       := $(CFILES:.c=.o) $(ASMFILES:.asm=.asm.o)
DEPS      := $(CFILES:.c=.d)

# User options.
DBGOUT = no
DBGSYM = no

PREFIX = $(shell pwd)

CC      = gcc
LD      = $(CC:gcc=ld)
AS      = nasm
QEMU    = qemu-system-x86_64

CFLAGS    = -O2 -pipe -Wall -Wextra
LDFLAGS   = -O2
QEMUFLAGS = -m 2G -enable-kvm -smp 4

# Flags for compilation.
BUILD_TIME := $(shell date)

CHARDFLAGS := $(CFLAGS)            \
	-DBUILD_TIME='"$(BUILD_TIME)"' \
	-std=gnu99                     \
	-masm=intel                    \
	-fno-pic                       \
	-mno-sse                       \
	-mno-sse2                      \
	-mno-mmx                       \
	-mno-80387                     \
	-mno-red-zone                  \
	-mcmodel=kernel                \
	-ffreestanding                 \
	-fno-stack-protector           \
	-fno-omit-frame-pointer        \
	-I.                            \
	-I$(LAI_DIR)/include

ifeq ($(DBGOUT), tty)
CHARDFLAGS := $(CHARDFLAGS) -D_DBGOUT_TTY_
else ifeq ($(DBGOUT), qemu)
CHARDFLAGS := $(CHARDFLAGS) -D_DBGOUT_QEMU_
else ifeq ($(DBGOUT), both)
CHARDFLAGS := $(CHARDFLAGS) -D_DBGOUT_TTY_ -D_DBGOUT_QEMU_
endif

ifeq ($(DBGSYM), yes)
CHARDFLAGS := $(CHARDFLAGS) -g -D_DEBUG_
endif

LDHARDFLAGS := $(LDFLAGS) -nostdlib -no-pie -T linker.ld -z max-page-size=0x1000

QEMUHARDFLAGS := $(QEMUFLAGS)          \
	-debugcon stdio                    \
	# -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no -device rtl8139,netdev=mynet0

.PHONY: symlist all prepare build install uninstall clean run

all: $(LAI_DIR)
ifeq ($(PULLREPOS), true)
	cd $(LAI_DIR) && git pull
else
	true # -- NOT PULLING LAI REPO -- #
endif
	$(MAKE) build

$(LAI_DIR):
	git clone $(LAI_URL) $(LAI_DIR)

$(KERNELELF): $(BINS) $(OBJ) symlist
	$(LD) $(LDHARDFLAGS) $(OBJ) symlist.o -o $@
	OBJDUMP=$(CC:-gcc:-objdump) ./gensyms.sh
	$(CC) -x c $(CHARDFLAGS) -c symlist.gen -o symlist.o
	$(LD) $(LDHARDFLAGS) $(OBJ) symlist.o -o $@

symlist:
	echo '#include <symlist.h>' > symlist.gen
	echo 'struct symlist_t symlist[] = {{0xffffffffffffffff,""}};' >> symlist.gen
	$(CC) -x c $(CHARDFLAGS) -c symlist.gen -o symlist.o

build: $(KERNELELF)

install: all
	install -d $(DESTDIR)$(PREFIX)/boot
	install $(KERNELBIN) $(DESTDIR)$(PREFIX)/boot/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/boot/$(KERNELBIN)

-include $(DEPS)

%.o: %.c
	$(CC) $(CHARDFLAGS) -MMD -c $< -o $@

%.bin: %.real
	$(AS) $< -f bin -o $@

%.asm.o: %.asm
	$(AS) $< -f elf64 -o $@

clean:
	rm -f symlist.gen symlist.o $(OBJ) $(BINS) $(KERNELELF) $(DEPS)

run:
	$(QEMU) $(QEMUHARDFLAGS)

format:
	find -not -path "./acpi/lai/*" -type f  -name "*.h" -exec clang-format -style=file -i {} \;
	find -not -path "./acpi/lai/*" -type f  -name "*.c" -exec clang-format -style=file -i {} \;
