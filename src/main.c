#include "hal.h"
#include "string.h"

#if defined(HOSTED)
# include <stdio.h>
#endif

extern int __startup_begin, __startup_end;
extern int __shutdown_begin, __shutdown_end;

static const char *run_fns[32];
static unsigned num_run_fns = 0;
static const char *needed_fns[32];
static unsigned num_needed_fns = 0;

static int fn_has_been_run(const char *c) {
  for (unsigned i = 0; i < num_run_fns; ++i)
    if (!strcmp(c, run_fns[i]))
      return 1;
  return 0;
}

static int need_fn(const char *only, const char *fn) {
  /* If we're loading all functions, we need this one. */
  if (!only) return 1;

  /* Are we supposed to only load this function? */
  if (!strcmp(only, fn)) return 1;

  /* Else see if it is in the list of needed (prerequisite) functions. */
  for (unsigned i = 0; i < num_needed_fns; ++i)
    if (!strcmp(fn, needed_fns[i]))
      return 1;
  return 0;
}

static int has_fn(const char *c, init_fini_fn_t *begin, init_fini_fn_t *end) {
  for (init_fini_fn_t *i = begin; i < end; ++i) {
    if (!strcmp(i->name, c))
      return 1;
  }
  return 0;
}

int run_startup_shutdown_functions(init_fini_fn_t *begin, init_fini_fn_t *end,
                                   const char *only) {
  int ok = 0;
  int cant_go = 0;
  int made_progress = 1;

  num_run_fns = 0;
  num_needed_fns = 0;

  while (made_progress) {
    made_progress = 0;

    for (init_fini_fn_t *i = begin; i < end; ++i) {
      if (fn_has_been_run(i->name) || !need_fn(only, i->name))
        continue;


      cant_go = 0;
      if (i->prerequisites) {
        for (const char **prereq = i->prerequisites; *prereq != NULL; ++prereq) {
          if (!fn_has_been_run(*prereq) && has_fn(*prereq, begin, end)) {
#if defined(HOSTED)
            printf("%s requires %s\n", i->name, *prereq);
#endif
            cant_go = 1;
            if (only) {
              needed_fns[num_needed_fns++] = *prereq;
              made_progress = 1;
            }
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
      if (i->fn)
        ok |= i->fn();
      made_progress = 1;
    }
  }

  /* If we made no more progress, we either succeeded or failed. */
  if (cant_go) ok = -2;
  return ok;
}

int main(int argc, char **argv) {
  const char *only = NULL;

  /* To function as a test harness, we may be asked to only load in
     functions that are (transitive) prerequisites of a particular
     function. Otherwise, we should load all functions. */
  if (argc >= 3 && !strcmp(argv[1], "only-run")) {
    only = argv[2];
  }

  /* Start by running the startup functions. */
  int success = run_startup_shutdown_functions((init_fini_fn_t*)&__startup_begin,
                                               (init_fini_fn_t*)&__startup_end,
                                               only);
#if defined(HOSTED)
  printf("Running startup functions, status = %d\n", success);
#endif

  /* Run the main function. */
  //  get_main_function()(argc, argv);

  /* Finish by running the shutdown functions. */
  success = run_startup_shutdown_functions((init_fini_fn_t*)&__shutdown_begin,
                                           (init_fini_fn_t*)&__shutdown_end,
                                           only);
#if defined(HOSTED)
  printf("Running shutdown functions, status = %d\n", success);
#endif
  return 0;
}
