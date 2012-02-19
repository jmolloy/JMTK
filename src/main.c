#include "hal.h"
#include "string.h"

#if defined(HOSTED)
# include <stdio.h>
#endif

extern int __startup_begin, __startup_end;
extern int __shutdown_begin, __shutdown_end;

typedef struct init_fini_state {
  /* If non-null, only run functions in the transitive 
     prerequisite closure rooted at this node. */
  const char *only;
  /* Set of functions that have been run. */
  const char *run_fns[32];
  unsigned num_run_fns;
  /* Set of functions that are in the transitive
     prerequisite closure rooted at 'only'. */
  const char *needed_fns[32];
  unsigned num_needed_fns;
  /* First and last init_fini_fn_t */
  init_fini_fn_t *begin, *end;
  /* Explanatory text to show in the console */
  const char *text;
} init_fini_state_t;


static int fn_has_been_run(init_fini_state_t *s, const char *c) {
  for (unsigned i = 0; i < s->num_run_fns; ++i)
    if (!strcmp(c, s->run_fns[i]))
      return 1;
  return 0;
}

static int need_fn(init_fini_state_t *s, const char *fn) {
  /* If we're loading all functions, we need this one. */
  if (!s->only) return 1;

  /* Are we supposed to only load this function? */
  if (!strcmp(s->only, fn)) return 1;

  /* Else see if it is in the list of needed (prerequisite) functions. */
  for (unsigned i = 0; i < s->num_needed_fns; ++i)
    if (!strcmp(fn, s->needed_fns[i]))
      return 1;
  return 0;
}

static int has_fn(init_fini_state_t *s, const char *c) {
  for (init_fini_fn_t *i = s->begin; i < s->end; ++i) {
    if (!strcmp(i->name, c))
      return 1;
  }
  return 0;
}

void log_status(int status, const char *name, init_fini_state_t *s) {
  write_console("[", 1);
  if (status == 0)
    write_console("\033[32m OK \033[0m", 13);
  else
    write_console("\033[31mFAIL\033[0m", 13);
  write_console("] ", 2);
  write_console(s->text, strlen(s->text));
  write_console(" ", 1);
  write_console(name, strlen(name));
  write_console("\n", 1);
}

int run_startup_shutdown_functions(init_fini_state_t *s) {
  int ok = 0;
  int cant_go = 0;
  int made_progress = 1;

  s->num_run_fns = 0;
  s->num_needed_fns = 0;

  while (made_progress) {
    made_progress = 0;

    for (init_fini_fn_t *i = s->begin; i < s->end; ++i) {
      if (fn_has_been_run(s, i->name) || !need_fn(s, i->name))
        continue;

      cant_go = 0;
      if (i->prerequisites) {
        for (const char **prereq = i->prerequisites; *prereq != NULL; ++prereq) {
          if (!fn_has_been_run(s, *prereq) && has_fn(s, *prereq)) {
#if defined(HOSTED)
            printf("%s requires %s\n", i->name, *prereq);
#endif
            cant_go = 1;
            if (s->only) {
              s->needed_fns[s->num_needed_fns++] = *prereq;
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
      s->run_fns[s->num_run_fns++] = i->name;
      int status = 0;
      if (i->fn)
        status = i->fn();
      ok |= status;

      log_status(status, i->name, s);

      made_progress = 1;
    }
  }

  /* If we made no more progress, we either succeeded or failed to
     load a prerequisite. */
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

  init_fini_state_t state;
  state.begin = (init_fini_fn_t*)&__startup_begin;
  state.end = (init_fini_fn_t*)&__startup_end;
  state.only = only;
  state.text = "Started";

  /* Start by running the startup functions. */
  int success = run_startup_shutdown_functions(&state);
#if defined(HOSTED)
  printf("Running startup functions, status = %d\n", success);
#endif

  /* Run the main function. */
  //  get_main_function()(argc, argv);

  /* Finish by running the shutdown functions. */
  state.begin = (init_fini_fn_t*)&__shutdown_begin;
  state.end = (init_fini_fn_t*)&__shutdown_end;
  state.only = only;
  state.text = "Stopped";

  success = run_startup_shutdown_functions(&state);
#if defined(HOSTED)
  printf("Running shutdown functions, status = %d\n", success);
#endif
  return 0;
}
