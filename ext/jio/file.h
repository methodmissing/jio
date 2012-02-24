#ifndef JIO_FILE_H
#define JIO_FILE_H

typedef struct {
    jfs_t *fs;
} jfs_wrapper;

#define GetJioFile(obj) \
    jfs_wrapper *file = NULL; \
    Data_Get_Struct(obj, jfs_wrapper, file); \
    if (!file) rb_raise(rb_eTypeError, "uninitialized JIO file handle!");

void _init_rb_jio_file();

#endif