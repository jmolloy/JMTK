
TARGET_FLAGS := -DX86=1 -m32 -ffreestanding -I$(BUILD)/src/x86 -nostdlib
TARGET_LINKFLAGS := -m32 -Tsrc/x86/link.ld -nostdlib -lgcc -n
CSOURCES := $(shell find src/x86 -type f -name "*.c") $(CSOURCES_TI)
SSOURCES := $(shell find src/x86 -type f -name "*.s") $(SSOURCES_TI)
TESTS    := $(shell find test/x86 -type f -name "*.c") $(TESTS_TI)
EXAMPLES := examples/ide.c  $(EXAMPLES_TI)
RUNNER   := scripts/run.py

ifndef SCANTABLE
    SCANTABLE := src/x86/en_US.scantable
endif

EXAMPLEIMGS := $(patsubst %.c,$(BUILD)/%.img,$(EXAMPLES))

all: $(BUILD)/kernel.img $(EXAMPLEIMGS)

src/x86/keyboard.c: $(BUILD)/src/x86/scantable.inc

$(BUILD)/src/x86/scantable.inc: scripts/scantable.py
	@echo "\033[1mGEN\033[0m  $@"
	@python scripts/scantable.py $(SCANTABLE) $@

$(BUILD)/%.s.o: %.s Makefile | setup_builddir
	@echo "\033[1mNASM\033[0m $<"
	@nasm -felf $< -o $@

$(BUILD)/%.img: $(BUILD)/% | setup_builddir
	@echo "\033[1mGEN\033[0m  $@"
	@python scripts/image.py src/ $< $@
