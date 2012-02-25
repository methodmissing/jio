#ifndef JIO_EXT_H
#define JIO_EXT_H

#include "libjio.h"
#include "trans.h"
#include "ruby.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Compiler specific */

#if defined(__GNUC__) && (__GNUC__ >= 3)
#define JIO_UNUSED __attribute__ ((unused))
#define JIO_NOINLINE __attribute__ ((noinline))
#else
#define JIO_UNUSED
#define JIO_NOINLINE
#endif

#include <jio_prelude.h>

#define JioAssertType(obj, type, desc) \
    if (!rb_obj_is_kind_of(obj,type)) \
        rb_raise(rb_eTypeError, "wrong argument type %s (expected %s): %s", rb_obj_classname(obj), desc, RSTRING_PTR(rb_obj_as_string(obj)));

#define AssertOffset(off) \
    Check_Type(off, T_FIXNUM); \
    if (off < jio_zero) rb_raise(rb_eArgError, "offset must be >= 0"); \

#define AssertLength(len) \
    Check_Type(len, T_FIXNUM); \
    if (len < jio_zero) rb_raise(rb_eArgError, "length must be >= 0"); \

#include <file.h>
#include <transaction.h>

extern VALUE mJio;
extern VALUE cJioFile;
extern VALUE cJioTransaction;

extern VALUE jio_zero;
extern VALUE jio_empty_view;

#endif