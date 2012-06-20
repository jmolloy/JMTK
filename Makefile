BUILD := build

CSOURCES_TI := $(shell find src src/adt -maxdepth 1 -type f -name "*.c")
SSOURCES_TI := $(shell find src src/adt -maxdepth 1 -type f -name "*.s")
TESTS_TI    := $(shell find test -maxdepth 1 -type f -name "*.c")
EXAMPLES_TI := examples/debugger.c examples/readline.c

all:

ifndef TARGET
    $(error TARGET is not set. Please set to one of HOSTED, X86 or X64.)
endif
ifeq ($(TARGET),HOSTED)
    TARGET_FLAGS := -DHOSTED=1
    TARGET_LINKFLAGS := 
    CSOURCES := $(shell find src/hosted -type f -name "*.c") $(CSOURCES_TI)
    SSOURCES := $(shell find src/hosted -type f -name "*.s") $(SSOURCES_TI)
    TESTS    := $(shell find test/hosted -type f -name "*.c") #$(TESTS_TI)
    EXAMPLES := $(EXAMPLES_TI)
endif
ifeq ($(TARGET),X86)
    TARGET_FLAGS := -DX86=1 -m32 -ffreestanding -I$(BUILD)/src/x86 -nostdlib
    TARGET_LINKFLAGS := -m32 -Tsrc/x86/link.ld -nostdlib -lgcc
    CSOURCES := $(shell find src/x86 -type f -name "*.c") $(CSOURCES_TI)
    SSOURCES := $(shell find src/x86 -type f -name "*.s") $(SSOURCES_TI)
    TESTS    := $(shell find test/x86 -type f -name "*.c") $(TESTS_TI)
    EXAMPLES := examples/ide.c  $(EXAMPLES_TI)
    RUNNER   := scripts/run.py

src/x86/keyboard.c: $(BUILD)/src/x86/scantable.inc

    ifndef SCANTABLE
        SCANTABLE := src/x86/en_US.scantable
    endif

    EXAMPLEIMGS := $(patsubst %.c,$(BUILD)/%.img,$(EXAMPLES))

    all: $(BUILD)/kernel.img $(EXAMPLEIMGS)

$(BUILD)/src/x86/scantable.inc: scripts/scantable.py
	@echo "\033[1mGEN\033[0m  $@"
	@python scripts/scantable.py $(SCANTABLE) $@

$(BUILD)/%.s.o: %.s Makefile | setup_builddir
	@echo "\033[1mNASM\033[0m $<"
	@nasm -felf $< -o $@

$(BUILD)/%.img: $(BUILD)/% | setup_builddir
	@echo "\033[1mGEN\033[0m  $@"
	@python scripts/image.py src/ $< $@

endif

COBJECTS := $(patsubst %.c,$(BUILD)/%.c.o,$(CSOURCES))
SOBJECTS := $(patsubst %.s,$(BUILD)/%.s.o,$(SSOURCES))
TESTEXES := $(patsubst %.c,$(BUILD)/%,$(TESTS))
EXAMPLEEXES := $(patsubst %.c,$(BUILD)/%,$(EXAMPLES))

CDEPS := $(patsubst %.c,$(BUILD)/%.c.d,$(CSOURCES))

.PHONY: all clean dist check

WARNINGS := -Wall -Wextra -Wno-unused-parameter
LINK_FLAGS := -Xlinker -n
DEFS := -g -std=c99 -nostdlibinc -fno-builtin $(WARNINGS) $(TARGET_FLAGS)

LINK_LIBK := -Wl,--whole-archive $(BUILD)/libk.a -Wl,--no-whole-archive

all: $(BUILD)/kernel $(TESTEXES) $(EXAMPLEEXES)

$(BUILD)/kernel: $(BUILD)/libk.a
	@echo "\033[1mLINK\033[0m   $(BUILD)/kernel"
	@$(CC) -o $(BUILD)/kernel $(LINK_LIBK) $(TARGET_LINKFLAGS)

$(BUILD)/libk.a: $(COBJECTS) $(SOBJECTS)
	@echo "\033[1mAR\033[0m   $(BUILD)/libk.a"
	@ar cr $(BUILD)/libk.a $?

-include $(CDEPS)

$(BUILD)/%.c.o: %.c Makefile | setup_builddir
	@echo "\033[1mCC\033[0m   $<"
	@$(CC) $(CFLAGS) $(DEFS) -MMD -MP -c $< -o $@ -I ./src/include -I ./src/third_party/include

$(BUILD)/%: %.c $(BUILD)/libk.a Makefile | setup_builddir
	@echo "\033[1mLINK\033[0m   $@"
	@$(CC) $(CFLAGS) $(DEFS) -MMD -MP $< -o $@ -I ./src/include $(LINK_LIBK) $(TARGET_LINKFLAGS)

setup_builddir:
	@mkdir -p $(BUILD)/src/x86
	@mkdir -p $(BUILD)/src/hosted
	@mkdir -p $(BUILD)/src/adt
	@mkdir -p $(BUILD)/src/third_party
	@mkdir -p $(BUILD)/test/x86
	@mkdir -p $(BUILD)/test/hosted
	@mkdir -p $(BUILD)/examples

clean:
	-@rm -r $(BUILD)

check: test

test: $(TESTEXES)
	-@fail=0; count=0; \
	for file in $(TESTS); do \
	    exe=`echo $$file | sed -e 's,\(.*\).c,$(BUILD)/\1,'`; \
	    echo -n "\033[1mTEST\033[0m   $$file"; \
	    bash $$file $(RUNNER) $$exe; \
	    case "$$?" in \
	        0) echo " -> \033[32mOK\033[0m" ;; \
	        *) echo " -> \033[31mFAIL\033[0m"; fail=`expr $$fail + 1` ;; \
	    esac; \
	    count=`expr $$count + 1`; \
	done; \
	echo; echo "Tests executed: $$count ($$fail failures)"