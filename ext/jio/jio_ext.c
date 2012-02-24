#include <jio_ext.h>

VALUE mJio;
VALUE cJioFile;
VALUE cJioTransaction;

VALUE jio_zero;
VALUE jio_empty_view;

#ifdef HAVE_RUBY_ENCODING_H
rb_encoding *binary_encoding;
#endif

/*
 *  call-seq:
 *     JIO.check("/path/file", JIO::J_CLEANUP)    =>  Hash
 *
 *  Checks and repairs a file previously created and managed through libjio.
 *
 * === Examples
 *     JIO.check("/path/file", JIO::J_CLEANUP)    =>  Hash
 *
*/

static VALUE rb_jio_s_check(VALUE jio, VALUE path, VALUE flags)
{
    int ret;
    VALUE result;
    struct jfsck_result res;
    Check_Type(path, T_STRING);
    Check_Type(flags, T_FIXNUM);
    ret = jfsck(RSTRING_PTR(path), NULL, &res, FIX2UINT(flags));
    if (ret == J_ENOMEM) rb_memerror();
    if (ret < 0) rb_sys_fail("jfsck");
    result = rb_hash_new();
    rb_hash_aset(result, rb_str_new2("total"), INT2NUM(res.total));
    rb_hash_aset(result, rb_str_new2("invalid"), INT2NUM(res.invalid));
    rb_hash_aset(result, rb_str_new2("in_progress"), INT2NUM(res.in_progress));
    rb_hash_aset(result, rb_str_new2("broken"), INT2NUM(res.broken));
    rb_hash_aset(result, rb_str_new2("corrupt"), INT2NUM(res.corrupt));
    rb_hash_aset(result, rb_str_new2("reapplied"), INT2NUM(res.reapplied));
    return result;
}

void
Init_jio_ext()
{
    mJio = rb_define_module("JIO");

/*
 *  Generic globals (Fixnum 0 and empty Array for blank transaction views)
 */
    jio_zero = INT2NUM(0);
    rb_gc_register_address(&jio_empty_view);
    jio_empty_view = rb_ary_new();

#ifdef HAVE_RUBY_ENCODING_H
    binary_encoding = rb_enc_find("binary");
#endif

/*
 *  Transaction specific constants
 */
    rb_define_const(mJio, "J_NOLOCK", INT2NUM(J_NOLOCK));
    rb_define_const(mJio, "J_NOROLLBACK", INT2NUM(J_NOROLLBACK));
    rb_define_const(mJio, "J_LINGER", INT2NUM(J_LINGER));
    rb_define_const(mJio, "J_COMMITTED", INT2NUM(J_COMMITTED));
    rb_define_const(mJio, "J_ROLLBACKED", INT2NUM(J_ROLLBACKED));
    rb_define_const(mJio, "J_ROLLBACKING", INT2NUM(J_ROLLBACKING));
    rb_define_const(mJio, "J_RDONLY", INT2NUM(J_RDONLY));

/*
 *  Journal check specific constants
 */
    rb_define_const(mJio, "J_ESUCCESS", INT2NUM(J_ESUCCESS));
    rb_define_const(mJio, "J_ENOENT", INT2NUM(J_ENOENT));
    rb_define_const(mJio, "J_ENOJOURNAL", INT2NUM(J_ENOJOURNAL));
    rb_define_const(mJio, "J_ENOMEM", INT2NUM(J_ENOMEM));
    rb_define_const(mJio, "J_ECLEANUP", INT2NUM(J_ECLEANUP));
    rb_define_const(mJio, "J_EIO", INT2NUM(J_EIO));

/*
 *  jfscheck specific constants
 */
    rb_define_const(mJio, "J_CLEANUP", INT2NUM(J_CLEANUP));

/*
 *  Open specific constants (POSIX)
 */
    rb_define_const(mJio, "RDONLY", INT2NUM(O_RDONLY));
    rb_define_const(mJio, "WRONLY", INT2NUM(O_WRONLY));
    rb_define_const(mJio, "RDWR", INT2NUM(O_RDWR));
    rb_define_const(mJio, "CREAT", INT2NUM(O_CREAT));
    rb_define_const(mJio, "EXCL", INT2NUM(O_EXCL));
    rb_define_const(mJio, "TRUNC", INT2NUM(O_TRUNC));
    rb_define_const(mJio, "APPEND", INT2NUM(O_APPEND));
    rb_define_const(mJio, "NONBLOCK", INT2NUM(O_NONBLOCK));
    rb_define_const(mJio, "NDELAY", INT2NUM(O_NDELAY));
    rb_define_const(mJio, "SYNC", INT2NUM(O_SYNC));
    rb_define_const(mJio, "ASYNC", INT2NUM(O_ASYNC));

/*
 *  lseek specific constants
 */
    rb_define_const(mJio, "SEEK_SET", INT2NUM(SEEK_SET));
    rb_define_const(mJio, "SEEK_CUR", INT2NUM(SEEK_CUR));
    rb_define_const(mJio, "SEEK_END", INT2NUM(SEEK_END));

/*
 *  JIO module methods
 */
    rb_define_module_function(mJio, "check", rb_jio_s_check, 2);

    _init_rb_jio_file();
    _init_rb_jio_transaction();
}