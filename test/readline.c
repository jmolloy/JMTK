// RUN: %compile -g %s -o %t && \
// RUN: grep '^// IN:' %s | \
// RUN: xargs echo -e | \
// RUN: sed -e 's, \?// IN: ,,g' | \
// RUN: %run %t only-run readline-test --timeout 2500 | \
// RUN: %FileCheck %s

#include "hal.h"
#include "stdio.h"
#include "string.h"
#include "readline.h"

int rl() {
  char buf[64];
  while (1) {
    readline(buf, 64, "> ", NULL);
    kprintf("## '%s'\n", buf);
    if (!strcmp(buf, "END"))
      return 0;
  }
  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", "x86/serial",
                          "hosted/console", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "readline-test",
  .prerequisites = p,
  .fn = &rl
};

// IN: hello\\n
// CHECK: ## 'hello'

// IN: world\\r\\n
// CHECK: ## 'world'

// \x08 == backspace
// IN: abc\\x08de\\n
// CHECK: ## 'abde'

// \033D == left
// IN: abcd\\033De\\n
// CHECK: 'abced'

// \033C == right
// IN: abcd\\033D\\033De\\033Cf\\n
// CHECK: 'abecfd'

// \x17 == C-W
// IN: one two three\\x17\\n
// CHECK: 'one two '

// IN: one two three\\033D\\033D\\033D\\033D\\033D\\x17\\n
// CHECK: 'one three'

// IN: one two three\\033D\\033D\\033D\\033D\\033D\\x17four \\n
// CHECK: 'one four three'

// \x0b == C-K
// IN: abcdef\\033D\\033D\\033D\\x0b\\n
// CHECK: 'abc'

// IN: abcdef\\033D\\033D\\033D\\x0bxy\\n
// CHECK: 'abcxy'

// IN: \\nEND\\n
