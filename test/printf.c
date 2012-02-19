// RUN: %compile -g %s -o %t && %t only-run printf-test | %FileCheck %s
// XFAIL: X86
// XFAIL: X64
// XFAIL: ARM

#if !defined(HOSTED)
# error This test must be run on a hosted kernel!
#endif

#include "printf.h"
#include "hal.h"
#include <stdio.h>

static int test() {
  char buf[64];
  char *fmt;

  // CHECK: hello, world!
  fmt = "hello, world!";
  ksnprintf(buf, 64, fmt);
  printf("%s\n", buf);
  
  // CHECK: hello, string world!
  fmt = "hello, %s!";
  ksnprintf(buf, 64, fmt, "string world");
  printf("%s\n", buf);

  // CHECK: hello, padding world       !
  fmt = "hello, %-20s!";
  ksnprintf(buf, 64, fmt, "padding world");
  printf("%s\n", buf);

  // CHECK: hello,        padding world!
  fmt = "hello, %020s!";
  ksnprintf(buf, 64, fmt, "padding world");
  printf("%s\n", buf);

  // CHECK: hello,        padding world!
  fmt = "hello, %20s!";
  ksnprintf(buf, 64, fmt, "padding world");
  printf("%s\n", buf);

  // CHECK: Don't truncate me!
  fmt = "%5s!";
  ksnprintf(buf, 64, fmt, "Don't truncate me");
  printf("%s\n", buf);

  // CHECK: 5
  fmt = "%x";
  ksnprintf(buf, 64, fmt, 5);
  printf("%s\n", buf);
  
  // CHECK: 0005
  fmt = "%04x";
  ksnprintf(buf, 64, fmt, 5);
  printf("%s\n", buf);

  // CHECK: .5   .
  fmt = ".%-04x.";
  ksnprintf(buf, 64, fmt, 5);
  printf("%s\n", buf);

  // CHECK: abcdef
  fmt = "%x";
  ksnprintf(buf, 64, fmt, 0xabcdef);
  printf("%s\n", buf);

  // CHECK: ABCDEF
  fmt = "%X";
  ksnprintf(buf, 64, fmt, 0xabcdef);
  printf("%s\n", buf);

  // CHECK: 0xabcdef
  fmt = "%#x";
  ksnprintf(buf, 64, fmt, 0xabcdef);
  printf("%s\n", buf);

  // CHECK: 0x0f
  fmt = "%#04x";
  ksnprintf(buf, 64, fmt, 0xf);
  printf("%s\n", buf);

  // CHECK: 0x000f
  fmt = "%#.4x";
  ksnprintf(buf, 64, fmt, 0xf);
  printf("%s\n", buf);

  // CHECK: .    0x000f.
  fmt = ".%#010.4x.";
  ksnprintf(buf, 64, fmt, 0xf);
  printf("%s\n", buf);
  
  // CHECK: 24
  fmt = "%d";
  ksnprintf(buf, 64, fmt, 24);
  printf("%s\n", buf);

  // CHECK: -24.
  fmt = "%d.";
  ksnprintf(buf, 64, fmt, -24);
  printf("%s\n", buf);

  // CHECK: -0024.
  fmt = "%05d.";
  ksnprintf(buf, 64, fmt, -24);
  printf("%s\n", buf);

  // CHECK: -24  .
  fmt = "%-05d.";
  ksnprintf(buf, 64, fmt, -24);
  printf("%s\n", buf);

  // CHECK: .     -0024.
  fmt = ".%010.4d.";
  ksnprintf(buf, 64, fmt, -24);
  printf("%s\n", buf);

  // CHECK: .-0024     .
  fmt = ".%-010.4d.";
  ksnprintf(buf, 64, fmt, -24);
  printf("%s\n", buf);

  // CHECK: .+24.
  fmt = ".%+d.";
  ksnprintf(buf, 64, fmt, 24);
  printf("%s\n", buf);

  // CHECK: . 24.
  fmt = ".% d.";
  ksnprintf(buf, 64, fmt, 24);
  printf("%s\n", buf);

  // CHECK: .+024.
  fmt = ".%+04d.";
  ksnprintf(buf, 64, fmt, 24);
  printf("%s\n", buf);

  // CHECK: .  +024.
  fmt = ".%+06.3d.";
  ksnprintf(buf, 64, fmt, 24);
  printf("%s\n", buf);

  // CHECK: .765.
  fmt = ".%o.";
  ksnprintf(buf, 64, fmt, 0765);
  printf("%s\n", buf);

  // CHECK: .0765.
  fmt = ".%#o.";
  ksnprintf(buf, 64, fmt, 0765);
  printf("%s\n", buf);

  // CHECK: .4294967295.
  fmt = ".%u.";
  ksnprintf(buf, 64, fmt, -1);
  printf("%s\n", buf);

  // CHECK: .0001.
  fmt = ".%0*d.";
  ksnprintf(buf, 64, fmt, 4, 1);
  printf("%s\n", buf);

  // CHECK: .  001.
  fmt = ".%0*.*d.";
  ksnprintf(buf, 64, fmt, 5, 3, 1);
  printf("%s\n", buf);

  // CHECK: .0004.
  fmt = ".%0*1$d.";
  ksnprintf(buf, 64, fmt, 4);
  printf("%s\n", buf);

  // CHECK: .  04.
  fmt = ".%0*2$.*d.";
  ksnprintf(buf, 64, fmt, 2, 4);
  printf("%s\n", buf);

  // CHECK: hello66world
  fmt = "%s%d%s";
  ksnprintf(buf, 64, fmt, "hello", 66, "world");
  printf("%s\n", buf);

  // CHECK: .a.
  fmt = ".%c.";
  ksnprintf(buf, 64, fmt, 'a');
  printf("%s\n", buf);

  // CHECK: .0x1234.
  fmt = ".%p.";
  ksnprintf(buf, 64, fmt, (void*)0x1234);
  printf("%s\n", buf);

  // CHECK: .0 20 1 40 2 60 3.
  fmt = ".%n %d %n %d %n %d %n.";
  ksnprintf(buf, 64, fmt, 20, 40, 60);
  printf("%s\n", buf);

  // CHECK: ..
  fmt = ".%.0x.";
  ksnprintf(buf, 64, fmt, 0);
  printf("%s\n", buf);

  return 0;
}

static init_fini_fn_t run_on_startup x = {
  .name = "printf-test",
  .prerequisites = NULL,
  .fn = &test
};
