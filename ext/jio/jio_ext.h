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

/*
 *  libjio wrapper structs
 */
typedef struct {
    jfs_t *fs;
} jfs_wrapper;

typedef struct {
    jtrans_t *trans;
    size_t view_capa;
    char **views;
    VALUE views_ary;
} jtrans_wrapper;

#endif