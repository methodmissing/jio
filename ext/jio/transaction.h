#ifndef JIO_TRANSACTION_H
#define JIO_TRANSACTION_H

typedef struct {
    jtrans_t *trans;
    size_t view_capa;
    char **views;
    VALUE views_ary;
} jtrans_wrapper;

#define JioAssertTransaction(obj) JioAssertType(obj, rb_cJioTransaction, "JIO::Transaction")
#define GetJioTransaction(obj) \
    jtrans_wrapper *trans = NULL; \
    JioAssertTransaction(obj); \
    Data_Get_Struct(obj, jtrans_wrapper, trans); \
    if (!trans) rb_raise(rb_eTypeError, "uninitialized JIO transaction handle!");

void rb_jio_mark_transaction(void *ptr);
void rb_jio_free_transaction(void *ptr);

VALUE rb_jio_file_new_transaction(VALUE obj, VALUE flags);

#endif