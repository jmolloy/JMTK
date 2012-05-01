/**#1

   Introduction
   ============

   So you want to make your own operating system, huh? You've searched around,
   found plenty of resources about scheduling algorithms, memory management
   techniques and "the way Linux does it", but still don't really have a clue how
   to get started or more importantly where those ideas fit in with the big
   picture?

   This set of tutorials/walkthroughs is designed to help you stitch together
   the theory and the practical and show you the links between the two, in the
   hopes that it'll "click" for you and you'll be able to go and research new ideas
   independently.

   If you've read the previous incarnation of my tutorial series, this will be a
   slight depart from that formula. While many liked them, there was derision
   from some corners for them being too "hand-holdy" - spending too much time on
   basic C instead of the concepts it was trying to teach.

   Therefore this is more of a walkthrough than a tutorial - I've created what I
   like to call a teaching kernel. It is a simple UNIX-like operating system
   that should be portable and maintainable (one of the things that was poor
   about my previous tutorials was that it was very difficult for me to
   maintain).

   It should ideally serve as a prop or aid to show the basic concepts of
   operating system implementation or as a base upon which to experiment or
   research, perhaps as part of an operating systems 101 course (I've had this
   request in the past).

   The goal of this chapter in particular is to explain the basics of how the
   kernel we will build is structured; in particular the build system used and
   the testing methodology.

   What is an operating system?
   ============================

   Before delving into creating a kernel, let's first step back and define what
   an *operating system* actually is.

   `Wikipedia <http://en.wikipedia.org/wiki/Operating_system>`_ says:

       An operating system (OS) is a set of software that manages computer
       hardware resources and provide common services for computer programs. The
       operating system is a vital component of the system software in a
       computer system. Application programs require an operating system to
       function.

   Linux (or GNU/Linux) is an operating system, as is Windows, as is Mac OS. An
   operating system provides access to hardware, memory management and task
   scheduling capabilities, but also importantly provide some sort of shell or
   interface with which a user can interact with the system.

   The canonical shell on a Linux system is a command shell such as /bin/sh,
   along with the standard utilities such as ``mv``, ``cp``, ``grep``
   etc. Without these, even with ``/bin/sh`` a user could not effectively
   interact with the system. On windows, the shell is the Windows desktop and
   Windows Explorer.

   So what is a kernel?
   --------------------

   A kernel is a part of an operating system. It runs in a privileged mode and provides and regulates access
   to hardware, task scheduling, memory management and the minimal set of
   functionality that nonprivileged applications require to work.

   The rest of the operating system sits atop this layer, exposing more complex
   functionality to the user and perhap decomposing it into simple calls to the
   kernel to actually do some work.

   For example, the command "mv a.txt b.txt" in a shell expands into several
   kernel calls:
 
   .. image:: doc/1.png

   The shell first calls the ``fork()`` in order to create a new child
   process. This process calls ``execve()`` to load the executable and start
   executing it. Once executing, ``mv`` will call ``stat()`` on both filenames
   to ensure the first exists and the second does not or exists and is not
   write-protected.

   If these both succeed, ``mv`` will then call ``rename()`` and exit, where the
   call to ``waitpid`` in the parent shell process will then return.

   Our aim in this tutorial is to create such a kernel, that a "userland" such
   as the GNU tools found in Linux or the BSD tools in {Open,Net}BSD can sit
   upon.

   This brings us nicely to the next question:

   Why UNIX?
   ---------

   As I've mentioned earlier, in this tutorial we are attempting to produce a
   *kernel*, not a full operating system. That would be a much larger scope of
   problem (although of course can be done).

   A kernel interacts with the rest of the system (OS and user applications) via
   some API, and in the case of UNIX this is clearly specified in the `POSIX
   specification <http://pubs.opengroup.org/onlinepubs/009695399/>`_ - if we
   stick to this API, we should be able to build and run applications written
   for other kernels on ours. This also aids my primary tutorial goal of showing
   how theory and practice combine to produce the familiar.

   Modularity
   ----------

   Moving slightly to kernel design, there are many schools on how to design
   a kernel, and they generally form a scale from
   "everything-built-together-in-one-big-blob" to
   "absolutely-everything-completely-separated":

     * *Monolithic* - Everything is built into one kernel binary.
     * *Modular* - All functionality that can realistically be is moved into
       modules that are loaded at boot time.
     * *Microkernel* - A very small kernel, and all functionality that can be
       (sometimes including memory management) moved out into nonprivileged
       applications called "servers". The idea being that if they fail they can
       be simply restarted and not take down the entire kernel.

   An example of a monolithic kernel is "classic" Linux - by classic I mean
   "Linux as it was in 1992 when Torvalds and Tanenbaum had their infamous
   `argument
   <http://en.wikipedia.org/wiki/Tanenbaum%E2%80%93Torvalds_debate>`_", and the
   counterexample of microkernel is Minix (written by Andrew Tanenbaum).

   It should be noted that there are very few kernels that fit directly into one
   of these categories. Much like true free market or communist economies can
   never really exist, most tend towards a hybrid design with an inclination one
   way or the other.

   Only if one is motivated by design ideals (Minix, GNU Hurd) or abnormal
   design constraints (QNX - hard real time, safety critical) does a design
   result that can be clearly categorised into one of the above.

   That said, if I were to categorise the kernel we produce in this set of
   tutorials it would be *modular monolithic* - everything is built into one
   binary, but inside the binary everything is modularised and initialised
   separately at runtime.

   I'm going to cut the OS theory short here - this isn't meant to be a full OS
   theory introduction - see `the OSDev.org wiki <http://wiki.osdev.org>`_ or
   Tanenbaum's Minix book for a fuller discussion.

   General project structure
   =========================

   The rigorous use of modules allows each chapter of the tutorial to be self
   standing, and ideally fully pluggable - you should be able to just take the
   "threading.c" file, add it to the src/ directory and then end up with a
   kernel that can multi-thread.

   Testing
   -------

   I'm not really going to mention this much during the tutorial text, but I
   have developed this kernel such that it can be tested as much as
   possible. For each target-specific feature (such as paging) I have stubbed up
   an implementation that should work on any computer as a hosted process under
   linux. All of this code is in the "hosted" subdirectories. This allows me
   (and you!) to
   test the target-agnostic parts such as the scheduler and heap in isolation
   and with a proper debugger and valgrind.

   Along with this we also have target tests, which are run via a python wrapper
   around qEmu. The test harness itself is
   `"Lit" <http://llvm.org/cmds/lit.html>`_, which is part of the LLVM compiler
   infrastructure and is used for their testing. It's extremely lightweight,
   robust and is an exceedingly good tool.

   In the tutorial documentation I will not explicitly mention the use of tests,
   but tests will have been written for every feature and chapter. Similarly I
   will keep the focus on the target in question (x86/x64/arm), and not mention
   the hosted build at all (except for this first chapter!).

   Code layout
   -----------

   The code is layed out something like this::

       .
       +-- doc
       +-- examples
       +-- scripts
       |   +-- elftools
       |   `-- pyelftools-0.20
       +-- src
       |   +-- adt
       |   +-- hosted
       |   +-- include
       |   +-- third_party
       |   `-- x86
       `-- test
           +-- lit
           `-- x86

   ``scripts`` holds a bunch of scripts for interacting with qEmu (see later),
   generating floppy disk image (again, see later) and manipulating the graph of
   chapter dependencies (for my use only ;) ).

   All the source is in the ``src`` subdirectory, with ``hosted`` holding code
   relating to hosted mode (stubbed linux subprocess), ``x86`` holding the x86
   and x64 specific code, and ``adt`` holding some abstract data types (such as
   an extensible bitmap) used elsewhere in the kernel.

   Build system
   ------------

   For building the kernel I use CMake. No holy wars please, it's just a build
   system! ;).

   The intended usage of the CMakeLists.txt provided is shown below - CMake can
   be rather irritating to learn so if you plan to code along, I suggest just
   for this tutorial you just download the skeleton provided at the end::

       mkdir build
       cd build
       cmake .. -DTARGET=Hosted

   Replace "Hosted" with "X86", "X64" or "ARM" for your preferred target.

   Code!
   =====

**/

#include "hal.h"
#include "string.h"

#if defined(HOSTED)
# include <stdio.h>
#endif

/**#4
   The first real thing we should do is define a structure to hold the state of
   running initialisation or finalisation functions. {*/
typedef struct init_fini_state {
  /* If non-null, only run functions in the transitive closure of prerequisites
     rooted at this node. */
  const char *only;

  /* Set of functions that have been run. */
  const char *run_fns[32];
  unsigned num_run_fns;

  /* Set of functions that are in the transitive closure of prerequisites
     rooted at 'only', if 'only' is set. */
  const char *needed_fns[32];
  unsigned num_needed_fns;

  /* First and last init_fini_fn_t */
  init_fini_fn_t *begin, *end;

  /* Explanatory text to show in the console - "Started" or "Stopped" */
  const char *text;
} init_fini_state_t;

/**
   I should mention here that the kernel is designed to have two modes - to
   function as a full kernel (in which case all modules are loaded) and to
   function as a test harness, in which case we may specify just one module to
   load and only want to load it and its dependencies (and its dependencies'
   dependencies and so on - referred to as the "transitive closure" of the
   dependencies).

   The structure has to hold the name of the specific module we're loading if in
   test harness mode, the functions that have been run already (``run_fns``), and the set of
   functions that we have discovered are required to be loaded (``needed_fns``).

   With this in place, lets next define the entry point to the kernel
   proper. Note that this is the target-agnostic entry point - there may (and
   will) be target-dependent startup code before we get to this point. {*/

/* Symbols defined by the linker script, the address of which can be treated as
   a init_fini_fn_t*. */
extern int __startup_begin, __startup_end;
extern int __shutdown_begin, __shutdown_end;

static int run_startup_shutdown_functions(init_fini_state_t *s);

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
  state.end   = (init_fini_fn_t*)&__startup_end;
  state.only  = only;
  state.text  = "Started";

  /* Start by running the startup functions. */
  int success = run_startup_shutdown_functions(&state);
  (void)success;
#if defined(HOSTED)
  printf("Running startup functions, status = %d\n", success);
#endif

  if (!only)
    debugger_trap(NULL);
  
  /* Finish by running the shutdown functions. */
  state.begin = (init_fini_fn_t*)&__shutdown_begin;
  state.end   = (init_fini_fn_t*)&__shutdown_end;
  state.only  = only;
  state.text  = "Stopped";

  success = run_startup_shutdown_functions(&state);
  (void)success;
#if defined(HOSTED)
  printf("Running shutdown functions, status = %d\n", success);
#endif
  return 0;
}

/**
   You may notice that this calls a few functions not yet defined.
   
     * ``run_startup_shutdown_functions`` will be defined shortly, and will walk
       the dependency tree and start running functions for either startup or
       shutdown.
     * ``debugger_trap`` is declared in ``hal.h``, and will launch the kernel
       debugger (or do nothing if the debugger is not implemented as it isn't at
       the moment). This is called after all modules have finished loading - it
       assumes that if we are not in test harness mode (``!only``) we don't want
       to immediately shut down if all modules complete loading successfully,
       and so traps to allow us to inspect the state of the kernel. **/

/**
   Lets define a few helper functions for use when implementing
   ``run_startup_shutdown_functions``. {*/

/* Has the function 'c' been run already? */
static int fn_has_been_run(init_fini_state_t *s, const char *c) {
  for (unsigned i = 0; i < s->num_run_fns; ++i)
    if (!strcmp(c, s->run_fns[i]))
      return 1;
  return 0;
}

/* Do we need to load the function 'fn'? */
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

/* Is the function 'c' available for loading? */
static int has_fn(init_fini_state_t *s, const char *c) {
  for (init_fini_fn_t *i = s->begin; i < s->end; ++i) {
    if (!strcmp(i->name, c))
      return 1;
  }
  return 0;
}

/* Write a message to the console. */
static void log_status(int status, const char *name, init_fini_state_t *s) {
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

/**
   This last function ``log_status`` uses another as-yet-undefined function
   ``write_console``. This again is declared in ``hal.h`` and allows access to
   the kernel console. It takes a character array and a length. We'll define it
   later. Until then, it'll be difficult to test this code unless we are on a
   hosted platform (running as a hosted process under linux, with -DHOSTED=1 and
   ``printf`` available!).

   Now we can define the grunt function which will run all module
   startup/shutdown functions in order. {*/

/* Run through all known functions in 's' and run them.

   If s->only is non-NULL, only run the function s->only and 
   any of its prerequisities (transitively). */
static int run_startup_shutdown_functions(init_fini_state_t *s) {
  int ok = 0;
  int cant_go = 0;
  int made_progress = 1;

  s->num_run_fns = 0;
  s->num_needed_fns = 0;

  /* Keep going while something changed. */
  while (made_progress) {
    made_progress = 0;

    for (init_fini_fn_t *i = s->begin; i < s->end; ++i) {
      if (fn_has_been_run(s, i->name) || !need_fn(s, i->name))
        continue;

      cant_go = 0;
      /* If i has prerequisites, iterate through them. */
      if (i->prerequisites) {
        for (const char **prereq = i->prerequisites; *prereq != NULL; ++prereq) {
          /* Has this function not already been run and do we have it? */
          if (!fn_has_been_run(s, *prereq) && has_fn(s, *prereq)) {
#if defined(HOSTED)
            printf("%s requires %s\n", i->name, *prereq);
#endif
            /* Prereq hasn't been run, so this function can't be run yet. */
            cant_go = 1;
            /* If we're only running one function and its prereqs, add this
               function to its prereq list. */
            if (s->only) {
              if (!need_fn(s, *prereq))
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
      /* Mark this function as having been run. */
      s->run_fns[s->num_run_fns++] = i->name;
      int status = 0;
      /* The function may be NULL - handle that gracefully. */
      if (i->fn)
        status = i->fn();
      ok |= status;

      log_status(status, i->name, s);

      made_progress = 1;
    }
  }

  /* If we made no more progress, we either succeeded or failed to load a
     prerequisite. */
  if (cant_go) ok = -2;

  return ok;
}

/**
   ... and that should be that. It can only be tested so far in hosted mode -
   configure with::

       cmake -DTARGET=Hosted
       make -j

   Then run ``./src/kernel``. In the next chapter we'll define startup code to
   allow booting natively baremetal. */
