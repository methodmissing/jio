#ifndef JIO_FILE_H
#define JIO_FILE_H

#define JIO_FILE_CLOSED 0x01

typedef struct {
    jfs_t *fs;
    int flags;
} jio_jfs_wrapper;

#define JioAssertFile(obj) JioAssertType(obj, rb_cJioFile, "JIO::File")
#define JioGetFile(obj) \
    jio_jfs_wrapper *file = NULL; \
    JioAssertFile(obj); \
    Data_Get_Struct(obj, jio_jfs_wrapper, file); \
    if (!file) rb_raise(rb_eTypeError, "uninitialized JIO file handle!");

void _init_rb_jio_file();

#endif