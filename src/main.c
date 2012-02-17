#include "hal.h"
#include "string.h"

#if defined(HOSTED)
# include <stdio.h>
#endif

extern int __startup_begin, __startup_end;
extern int __shutdown_begin, __shutdown_end;

static const char *run_fns[32];
static unsigned num_run_fns = 0;

static int fn_has_been_run(const char *c) {
  for (unsigned i = 0; i < num_run_fns; ++i)
    if (!strcmp(c, run_fns[i]))
      return 1;
  return 0;
}

int run_startup_shutdown_functions(init_fini_fn_t *begin, init_fini_fn_t *end) {
  int ok = 0;
  int cant_go = 0;
  int made_progress = 1;

  num_run_fns = 0;

  while (made_progress) {
    made_progress = 0;

    for (init_fini_fn_t *i = begin; i < end; ++i) {
      if (fn_has_been_run(i->name))
        continue;

      cant_go = 0;
      if (i->prerequisites) {
        for (const char **prereq = i->prerequisites; *prereq != NULL; ++prereq) {
          if (!fn_has_been_run(*prereq)) {
#if defined(HOSTED)
            printf("%s requires %s\n", i->name, *prereq);
#endif
            cant_go = 1;
            break;
          }
        }
        /* Prerequisite has not been run yet, bail out. */
        if (cant_go) continue;
      }

#if defined(HOSTED)
      printf("Running %s...\n", i->name);
#endif
      run_fns[num_run_fns++] = i->name;
      ok |= i->fn();
      made_progress = 1;
    }
  }

  /* If we made no more progress, we either succeeded or failed. */
  if (cant_go) ok = -2;
  return ok;
}

int main(int argc, char **argv) {
  /* Start by running the startup functions. */
  int success = run_startup_shutdown_functions((init_fini_fn_t*)&__startup_begin,
                                               (init_fini_fn_t*)&__startup_end);
#if defined(HOSTED)
  printf("Running startup functions, status = %d\n", success);
#endif

  /* Run the main function. */
  //  get_main_function()(argc, argv);

  /* Finish by running the shutdown functions. */
  success = run_startup_shutdown_functions((init_fini_fn_t*)&__shutdown_begin,
                                           (init_fini_fn_t*)&__shutdown_end);
#if defined(HOSTED)
  printf("Running shutdown functions, status = %d\n", success);
#endif
  return 0;
}
