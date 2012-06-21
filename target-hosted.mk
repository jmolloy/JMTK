TARGET_FLAGS := -DHOSTED=1
TARGET_LINKFLAGS := 
CSOURCES := $(shell find src/hosted -type f -name "*.c") $(CSOURCES_TI)
SSOURCES := $(shell find src/hosted -type f -name "*.s") $(SSOURCES_TI)
TESTS    := $(shell find test/hosted -type f -name "*.c") #$(TESTS_TI)
EXAMPLES := $(EXAMPLES_TI)
