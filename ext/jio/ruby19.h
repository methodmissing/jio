#ifndef JIO_RUBY19_H
#define JIO_RUBY19_H

#include <ruby/encoding.h>
#include <ruby/io.h>
extern rb_encoding *binary_encoding;
#define JioEncode(str) rb_enc_associate(str, binary_encoding)
#ifndef THREAD_PASS
#define THREAD_PASS rb_thread_schedule();
#endif

#define TRAP_BEG
#define TRAP_END

#endif