#ifndef ASSERT_H
#define ASSERT_H

#if defined(HOSTED)
# include_next <assert.h>
#else

#define _stringify(x) (#x)
#define assert(cond) ( (cond) ? (void)0 : assert_fail(#cond, __FILE__, __LINE__) )

#endif

#endif
