#ifndef JIO_RUBINIUS_H
#define JIO_RUBINIUS_H

#define RSTRING_NOT_MODIFIED

#ifdef HAVE_RUBY_ENCODING_H
#include <ruby/st.h>
#include <ruby/encoding.h>
#include <ruby/io.h>
extern rb_encoding *binary_encoding;
#define JioEncode(str) rb_enc_associate(str, binary_encoding)
#else
#include "st.h"
#define JioEncode(str) str
#endif

#define TRAP_BEG
#define TRAP_END

#define THREAD_PASS rb_thread_schedule();

#endif