#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "thread.h"

void scheduler_ready(thread_t *t);
thread_t *scheduler_next();

#endif
