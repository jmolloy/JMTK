BUILD := build

CSOURCES_TI := $(shell find src src/adt -maxdepth 1 -type f -name "*.c")
SSOURCES_TI := $(shell find src src/adt -maxdepth 1 -type f -name "*.s")
TESTS_TI    := $(shell find test -maxdepth 1 -type f -name "*.c")

ifndef TARGET
    $(error TARGET is not set. Please set to one of HOSTED, X86 or X64.)
endif
ifeq ($(TARGET),HOSTED)
    TARGET_FLAGS := -DHOSTED=1
    TARGET_LINKFLAGS := 
    CSOURCES := $(shell find src/hosted -type f -name "*.c") $(CSOURCES_TI)
    SSOURCES := $(shell find src/hosted -type f -name "*.s") $(SSOURCES_TI)
    TESTS    := $(TESTS_TI)
endif

COBJECTS := $(patsubst %.c,$(BUILD)/%.c.o,$(CSOURCES))
SOBJECTS := $(patsubst %.s,$(BUILD)/%.s.o,$(SSOURCES))
TESTEXES := $(patsubst %.c,$(BUILD)/%,$(TESTS))

CDEPS := $(patsubst %.c,$(BUILD)/%.c.d,$(CSOURCES))

.PHONY: all clean dist check

WARNINGS := -Wall -Wextra -Wno-unused-parameter
DEFS := -g -std=c99 -nostdlibinc -fno-builtin $(WARNINGS) $(TARGET_FLAGS)

LINK_LIBK := -Wl,--whole-archive $(BUILD)/libk.a -Wl,--no-whole-archive

all: $(BUILD)/kernel $(TESTEXES)

$(BUILD)/kernel: $(BUILD)/libk.a
	@echo "LINK   $(BUILD)/kernel"
	$(CC) -o $(BUILD)/kernel $(LINK_LIBK) $(TARGET_LINKFLAGS)

$(BUILD)/libk.a: $(COBJECTS) $(SOBJECTS)
	@echo "AR   $(BUILD)/libk.a"
	@ar cr $(BUILD)/libk.a $?

-include $(CDEPS)

$(BUILD)/%.c.o: %.c Makefile | setup_builddir
	@echo "CC   $<"
	@$(CC) $(CFLAGS) $(DEFS) -MMD -MP -c $< -o $@ -I ./src/include

$(BUILD)/%: %.c Makefile | setup_builddir
	@echo "LINK   $@"
	@$(CC) $(CFLAGS) $(DEFS) -MMD -MP $< -o $@ -I ./src/include $(LINK_LIBK) $(TARGET_LINKFLAGS)

setup_builddir:
	@mkdir -p $(BUILD)/src/x86
	@mkdir -p $(BUILD)/src/hosted
	@mkdir -p $(BUILD)/src/adt
	@mkdir -p $(BUILD)/src/third_party
	@mkdir -p $(BUILD)/test/x86
	@mkdir -p $(BUILD)/test/hosted
