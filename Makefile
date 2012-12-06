BUILD := build

CSOURCES_TI := $(shell find src src/adt -maxdepth 1 -type f -name "*.c")
SSOURCES_TI := $(shell find src src/adt -maxdepth 1 -type f -name "*.s")
TESTS_TI    := $(shell find test -maxdepth 1 -type f -name "*.c")
EXAMPLES_TI := examples/debugger.c examples/readline.c examples/mount.c

all:

ifndef TARGET
    # Not a fatal error if the make target is "clean" or "doc"
    ifeq ($(MAKECMDGOALS),clean)
    else ifeq ($(MAKECMDGOALS),doc)
    else
        $(error TARGET is not set. Please set to one of HOSTED, X86 or X64. $(MAKECMDGOALS)x)
    endif
endif

TARGETL := $(shell echo $(TARGET) | tr '[:upper:]' '[:lower:]')
TARGETU := $(shell echo $(TARGET) | tr '[:lower:]' '[:upper:]')

-include target-$(TARGETL).mk

COBJECTS := $(patsubst %.c,$(BUILD)/%.c.o,$(CSOURCES))
SOBJECTS := $(patsubst %.s,$(BUILD)/%.s.o,$(SSOURCES))
TESTEXES := $(patsubst %.c,$(BUILD)/%,$(TESTS))
EXAMPLEEXES := $(patsubst %.c,$(BUILD)/%,$(EXAMPLES))

DEBUG_DEFS := $(shell echo $(DEBUG) | sed -e 's/^\|,/ -DDEBUG_/g')

CDEPS := $(patsubst %.c,$(BUILD)/%.c.d,$(CSOURCES))

.PHONY: all clean dist check doc rst html

WARNINGS := -Wall -Wextra -Wno-unused-parameter
LINK_FLAGS := -Xlinker -n
DEFS := -O0 -g -std=c99 -nostdlibinc -fno-builtin $(WARNINGS) $(TARGET_FLAGS) $(DEBUG_DEFS)

LINK_LIBK := -Wl,--whole-archive $(BUILD)/libk.a -Wl,--no-whole-archive

all: $(BUILD)/kernel $(TESTEXES) $(EXAMPLEEXES)

$(BUILD)/kernel: $(BUILD)/libk.a
	@echo "\033[1mLINK\033[0m $(BUILD)/kernel"
	@$(CC) -o $(BUILD)/kernel $(LINK_LIBK) $(TARGET_LINKFLAGS)

$(BUILD)/libk.a: $(COBJECTS) $(SOBJECTS)
	@echo "\033[1mAR\033[0m   $(BUILD)/libk.a"
	@ar cr $(BUILD)/libk.a $?

-include $(CDEPS)

$(BUILD)/%.c.o: %.c Makefile | setup_builddir
	@echo "\033[1mCC\033[0m   $<"
	@$(CC) $(CFLAGS) $(DEFS) -MMD -MP -c $< -o $@ -I ./src/include -I ./src/third_party/include

$(BUILD)/%: %.c $(BUILD)/libk.a Makefile | setup_builddir
	@echo "\033[1mLINK\033[0m $@"
	@$(CC) $(WARNINGS) $(DEFS) -MMD -MP $< -o $@ -I ./src/include $(LINK_LIBK) $(TARGET_LINKFLAGS)

$(BUILD)/filecheck: utils/filecheck.c Makefile | setup_builddir
	$(CC) -g -std=c99 $(WARNINGS) -DHOSTED -DSTANDALONE -MMD -MP utils/filecheck.c -o $@ -I ./src/include

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

rst: layout.graph $(CSOURCES) $(SSOURCES) src/x86/link.ld scripts/docs.py
	python scripts/docs.py --template doc/template.html --graph layout.graph --output-dir build/doc/rst

html: rst doc/conf.py
	PYTHONPATH=doc/sphinx_extensions:$PYTHONPATH sphinx-build -c doc -b html build/doc/rst build/doc/html

doc: html
