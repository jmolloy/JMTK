#if 0
exit `grep '^// IN:' $0 | xargs echo -e | sed -e 's, \?// IN: ,,g' | $1 $2 | ./test/FileCheck $0`
#endif

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

static prereq_t p[] = { {"x86/screen",NULL}, {"x86/keyboard",NULL},
                        {"x86/serial",NULL}, {"hosted/console",NULL},
                        {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "readline-test",
  .load_after = p,
  .required = NULL,
  .init = &rl,
  .fini = NULL
};
module_t *test_module = &x;

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

// Start testing history...

// IN: a\\n
// IN: b\\n
// IN: c\\n
// IN: \\033A\\n
// CHECK: 'c'

// IN: \\033A\\033A\\033A\\033A\\033Bg\\n
// CHECK: 'bg'

// IN: \\033A\\033Bs\\n
// CHECK: 's'

// IN: abc\\x01d\\n
// CHECK: 'dabc'

// IN: abc\\x01d\\x05e\\n
// CHECK: 'dabce'

// IN: abc\\x03\\n
// CHECK: ''

// IN: \\nEND\\n
