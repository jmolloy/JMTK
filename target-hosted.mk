TARGET_FLAGS := -DHOSTED=1
TARGET_LINKFLAGS := 
CSOURCES := $(shell find src/hosted -type f -name "*.c") $(CSOURCES_TI)
SSOURCES := $(shell find src/hosted -type f -name "*.s") $(SSOURCES_TI)
TESTS    := $(shell find test/hosted -type f -name "*.c") $(TESTS_TI)
EXAMPLES := $(EXAMPLES_TI)

ifdef COVERAGE
  LINK_FLAGS := $(LINK_FLAGS) -fprofile-arcs -ftest-coverage
endif

$(BUILD)/%.s.o: %.s Makefile | setup_builddir
	@echo "\033[1mNASM\033[0m $<"
	@nasm -felf64 $< -o $@
