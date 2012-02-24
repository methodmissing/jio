#ifndef JIO_JRUBY_H
#define JIO_JRUBY_H

#include "st.h"

/* XXX */
#define JioEncode(str) str
#ifndef THREAD_PASS
#define THREAD_PASS rb_thread_schedule();
#endif

#define TRAP_BEG
#define TRAP_END

#undef rb_errinfo
#define rb_errinfo() (rb_gv_get("$!"))

#define HAVE_RB_THREAD_BLOCKING_REGION 1

#endif