#ifndef JIO_TRANSACTION_H
#define JIO_TRANSACTION_H

typedef struct {
    jtrans_t *trans;
    size_t view_capa;
    char **views;
    VALUE views_ary;
} jtrans_wrapper;

#define GetJioTransaction(obj) \
    jtrans_wrapper *trans = NULL; \
    Data_Get_Struct(obj, jtrans_wrapper, trans); \
    if (!trans) rb_raise(rb_eTypeError, "uninitialized JIO transaction handle!");

void rb_jio_mark_transaction(jtrans_wrapper *trans);
void rb_jio_free_transaction(jtrans_wrapper *trans);

VALUE rb_jio_file_new_transaction(VALUE obj, VALUE flags);

#endif