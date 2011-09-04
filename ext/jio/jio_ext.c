#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "../libjio/libjio/libjio.h"
#include "../libjio/libjio/trans.h"
#include "ruby.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 *  MRI version agnostic macros
 */
#ifdef RUBY_VM
#include <ruby/encoding.h>
#include <ruby/io.h>
static rb_encoding *binary_encoding;
#define JioEncode(str) rb_enc_associate(str, binary_encoding)
#define TRAP_BEG
#define TRAP_END
#else
#define JioEncode(str) str
#include "rubyio.h"
#include "rubysig.h"
#ifndef RSTRING_PTR
#define RSTRING_PTR(str) RSTRING(str)->ptr
#endif
#ifndef RSTRING_LEN
#define RSTRING_LEN(s) (RSTRING(s)->len)
#endif
#endif

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

VALUE mJio;
VALUE cJioFile;
VALUE cJioTransaction;

static VALUE jio_zero;
static VALUE jio_empty_view;

/*
 *  libjio struct -> object macros
 */
#define GetJioFile(obj) \
    jfs_wrapper *file = NULL; \
    Data_Get_Struct(obj, jfs_wrapper, file); \
    if (!file) rb_raise(rb_eTypeError, "uninitialized JIO file handle!");

#define GetJioTransaction(obj) \
    jtrans_wrapper *trans = NULL; \
    Data_Get_Struct(obj, jtrans_wrapper, trans); \
    if (!trans) rb_raise(rb_eTypeError, "uninitialized JIO transaction handle!");

/*
 *  Assertion macros
 */
#define AssertOffset(off) \
    Check_Type(off, T_FIXNUM); \
    if (off < jio_zero) rb_raise(rb_eArgError, "offset must be >= 0"); \

#define AssertLength(len) \
    Check_Type(len, T_FIXNUM); \
    if (len < jio_zero) rb_raise(rb_eArgError, "length must be >= 0"); \

/*
 *  Generic transaction error handler
 */
static inline VALUE transaction_result(ssize_t ret, const char *ctx)
{
    char err_buf[BUFSIZ];
    if (ret >= 0) return Qtrue;
    if (ret == -1) snprintf(err_buf, BUFSIZ, "JIO transaction error on %s (atomic warranties preserved)", ctx);
    if (ret == -2) snprintf(err_buf, BUFSIZ, "JIO transaction error on %s (atomic warranties broken)", ctx);
    rb_sys_fail(err_buf);
    return IN2NUM(ret);
}

/*
 *  GC callbacks for JIO::File
 */
static void rb_jio_mark_file(jfs_wrapper *file)
{
}

static void rb_jio_free_file(jfs_wrapper *file)
{
}

/*
 *  call-seq:
 *     JIO.open("/path/file", JIO::CREAT | JIO::RDWR, 0600, JIO::J_LINGER)    =>  JIO::File
 *
 *  Returns a handle to a journaled file instance. Same semantics as the UNIX open(2) libc call, with
 *  an additional one for libjio specific flags.
 *
 * === Examples
 *     JIO.open("/path/file", JIO::CREAT | JIO::RDWR, 0600, JIO::J_LINGER)    =>  JIO::File
 *
*/

static VALUE rb_jio_s_open(VALUE jio, VALUE path, VALUE flags, VALUE mode, VALUE jflags)
{
    VALUE obj;
    jfs_wrapper *file = NULL;
    Check_Type(path, T_STRING);
    Check_Type(flags, T_FIXNUM);
    Check_Type(mode, T_FIXNUM);
    Check_Type(jflags, T_FIXNUM);
    obj = Data_Make_Struct(cJioFile, jfs_wrapper, rb_jio_mark_file, rb_jio_free_file, file);
    TRAP_BEG;
    file->fs = jopen(RSTRING_PTR(path), FIX2INT(flags), FIX2INT(mode), FIX2UINT(jflags));
    TRAP_END;
    if (file->fs == NULL) {
        xfree(file);
        rb_sys_fail("jopen");
    }
    rb_obj_call_init(obj, 0, NULL);
    return obj;
}

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

/*
 *  GC callbacks for JIO::Transaction
 */
static void rb_jio_mark_transaction(jtrans_wrapper *trans)
{
    rb_gc_mark(trans->views_ary);
}

static void rb_jio_free_transaction_views(jtrans_wrapper *trans)
{
    int i;
    char *buf = NULL;
    if (!NIL_P(trans->views_ary)) return;
    for (i = 0; i < trans->view_capa; ++i) {
       buf = trans->views[i];
       if (buf != NULL) xfree(buf);
    }
    xfree(trans->views);
}

static void rb_jio_free_transaction(jtrans_wrapper *trans)
{
    rb_jio_free_transaction_views(trans);
}

/*
 *  call-seq:
 *     file.transaction(JIO::J_LINGER)    =>  JIO::Transaction
 *
 *  Creates a new low level transaction from a libjio file reference.
 *
 * === Examples
 *     file.transaction(JIO::J_LINGER)    =>  JIO::Transaction
 *
*/

static VALUE rb_jio_file_new_transaction(VALUE obj, VALUE flags)
{
    VALUE transaction;
    jtrans_wrapper *trans = NULL;
    GetJioFile(obj);
    Check_Type(flags, T_FIXNUM);
    transaction = Data_Make_Struct(cJioTransaction, jtrans_wrapper, rb_jio_mark_transaction, rb_jio_free_transaction, trans);
    TRAP_BEG;
    trans->trans = jtrans_new(file->fs, FIX2INT(flags));
    TRAP_END;
    if (trans->trans == NULL) {
        xfree(trans);
        rb_sys_fail("jtrans_new");
    }
    trans->views = NULL;
    trans->view_capa = 0;
    trans->views_ary = Qnil;
    rb_obj_call_init(transaction, 0, NULL);
    return transaction;
}

/*
 *  call-seq:
 *     transaction.read(2, 2)    =>  boolean
 *
 *  Reads X bytes from Y offset into a transaction view. The buffered data is only visible once the
 *  transaction has been committed.
 *
 * === Examples
 *     transaction.read(2, 2)    =>  boolean
 *
*/

static VALUE rb_jio_transaction_read(VALUE obj, VALUE length, VALUE offset)
{
    int ret;
    char *buf = NULL;
    ssize_t len;
    GetJioTransaction(obj);
    AssertLength(length);
    AssertOffset(offset);
    len = (ssize_t)FIX2LONG(length);
    buf = xmalloc(len + 1);
    if (buf == NULL) rb_memerror();
    TRAP_BEG;
    ret = jtrans_add_r(trans->trans, buf, len, (off_t)NUM2OFFT(offset));
    TRAP_END;
    if (ret == -1) {
       xfree(buf);
       rb_sys_fail("jtrans_add_r");
    }
    trans->views = (char**)xrealloc(trans->views, (trans->view_capa += 1) * sizeof(char*));
    trans->views[trans->view_capa - 1] = buf;
    return Qtrue;
}

/*
 *  call-seq:
 *     transaction.views    =>  Array
 *
 *  Returns a sequence of buffers representing previous transactional read operations. The result is
 *  always an empty Array if the transaction hasn't been committed yet. Operations will be applied in
 *  order, and overlapping operations are permitted, in which case the latest one will prevail.
 *
 * === Examples
 *     transaction.views    =>  Array
 *
*/

static VALUE rb_jio_transaction_views(VALUE obj)
{
    jtrans_t *t = NULL;
    int i;
    GetJioTransaction(obj);
    t = trans->trans;
    if (t->flags & J_COMMITTED) {
        if (!NIL_P(trans->views_ary)) return trans->views_ary;
        if (trans->views != NULL) {
            trans->views_ary = rb_ary_new();
            for (i = 0; i < trans->view_capa; ++i) {
               rb_ary_push(trans->views_ary, JioEncode(rb_str_new2(trans->views[i])));
            }
            rb_jio_free_transaction_views(trans);
            return trans->views_ary;
        }
    }
    return jio_empty_view;
}

/*
 *  call-seq:
 *     transaction.write("data", 2)    =>  boolean
 *
 *  Spawns a write operation from a given buffer to X offset for this transaction. Only written to disk
 *  when the transaction has been committed. Operations will be applied in order, and overlapping
 *  operations are permitted, in which case the latest one will prevail.
 *
 * === Examples
 *     transaction.write("data", 2)    =>  boolean
 *
*/

static VALUE rb_jio_transaction_write(VALUE obj, VALUE buf, VALUE offset)
{
    int ret;
    GetJioTransaction(obj);
    Check_Type(buf, T_STRING);
    AssertOffset(offset);
    TRAP_BEG;
    ret = jtrans_add_w(trans->trans, StringValueCStr(buf), (size_t)RSTRING_LEN(buf), (off_t)NUM2OFFT(offset));
    TRAP_END;
    if (ret == -1) rb_sys_fail("jtrans_add_w");
    return Qtrue;
}

/*
 *  call-seq:
 *     transaction.commit    =>  boolean
 *
 *  Reads / writes all operations for this transaction to / from disk, in the order they were added.
 *  After this function returns successfully, all the data can be trusted to be on the disk. The commit
 *  is atomic with regards to other processes using libjio, but not accessing directly to the file.
 *
 * === Examples
 *     transaction.commit    =>  boolean
 *
*/

static VALUE rb_jio_transaction_commit(VALUE obj)
{
    ssize_t ret;
    GetJioTransaction(obj);
    TRAP_BEG;
    ret = jtrans_commit(trans->trans);
    TRAP_END;
    return transaction_result(ret, "commit");
}

/*
 *  call-seq:
 *     transaction.rollback    =>  boolean
 *
 *  This function atomically undoes a previous committed transaction. After its successful return, the
 *  data can be trusted to be on disk. The read operations will be ignored as we only care about on disk
 *  consistency.
 *
 * === Examples
 *     transaction.rollback    =>  boolean
 *
*/

static VALUE rb_jio_transaction_rollback(VALUE obj)
{
    ssize_t ret;
    VALUE res;
    GetJioTransaction(obj);
    TRAP_BEG;
    ret = jtrans_rollback(trans->trans);
    TRAP_END;
    res = transaction_result(ret, "rollback");
    if (trans->views != NULL) rb_jio_free_transaction_views(trans);
    return res;
}

/*
 *  call-seq:
 *     transaction.release    =>  nil
 *
 *  Free all transaction state and operation buffers
 *
 * === Examples
 *     transaction.release    =>  nil
 *
*/

static VALUE rb_jio_transaction_release(VALUE obj)
{
    GetJioTransaction(obj);
    TRAP_BEG;
    jtrans_free(trans->trans);
    TRAP_END;
    return Qnil;
}

/*
 *  call-seq:
 *     transaction.committed?    =>  boolean
 *
 *  Determines if this transaction has been committed.
 *
 * === Examples
 *     transaction.committed?    =>  boolean
 *
*/

static VALUE rb_jio_transaction_committed_p(VALUE obj)
{
    jtrans_t *t = NULL;
    GetJioTransaction(obj);
    t = trans->trans;
    return (t->flags & J_COMMITTED) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     transaction.rollbacked?    =>  boolean
 *
 *  Determines if this transaction has been rolled back.
 *
 * === Examples
 *     transaction.rollbacked?    =>  boolean
 *
*/

static VALUE rb_jio_transaction_rollbacked_p(VALUE obj)
{
    jtrans_t *t = NULL;
    GetJioTransaction(obj);
    t = trans->trans;
    return (t->flags & J_ROLLBACKED) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     transaction.rollbacking?    =>  boolean
 *
 *  Determines if this transaction is in the process of being rolled back.
 *
 * === Examples
 *     transaction.rollbacking?    =>  boolean
 *
*/

static VALUE rb_jio_transaction_rollbacking_p(VALUE obj)
{
    jtrans_t *t = NULL;
    GetJioTransaction(obj);
    t = trans->trans;
    return (t->flags & J_ROLLBACKING) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     file.sync    =>  boolean
 *
 *  Sync a file to disk. Makes sense only when using lingering transactions.
 *
 * === Examples
 *     file.sync    =>  boolean
 *
*/

static VALUE rb_jio_file_sync(VALUE obj)
{
    GetJioFile(obj);
    TRAP_BEG;
    return (jsync(file->fs) == 0) ? Qtrue : Qfalse;
    TRAP_END;
}

/*
 *  call-seq:
 *     file.close    =>  boolean
 *
 *  After a call to this method, the memory allocated for the open file will be freed. If there was an
 *  autosync thread started for this file, it will be stopped.
 *
 * === Examples
 *     file.close    =>  boolean
 *
*/

static VALUE rb_jio_file_close(VALUE obj)
{
    GetJioFile(obj);
    TRAP_BEG;
    return (jclose(file->fs) == 0) ? Qtrue : Qfalse;
    TRAP_END;
}

/*
 *  call-seq:
 *     file.move_journal("/path")    =>  boolean
 *
 *  Changes the location of the journal direction. The file cannot be in use at this time.
 *
 * === Examples
 *     file.move_journal("/path")    =>  boolean
 *
*/

static VALUE rb_jio_file_move_journal(VALUE obj, VALUE path)
{
    GetJioFile(obj);
    Check_Type(path, T_STRING);
    TRAP_BEG;
    return (jmove_journal(file->fs, RSTRING_PTR(path)) == 0) ? Qtrue : Qfalse;
    TRAP_END;
}

/*
 *  call-seq:
 *     file.autosync(5, 4000)    =>  boolean
 *
 *  Syncs to disk every X seconds, or every Y bytes written. Only one autosync thread per open file is
 *  allowed. Only makes sense with lingering transactions.
 *
 * === Examples
 *     file.autosync(5, 4000)    =>  boolean
 *
*/

static VALUE rb_jio_file_autosync(VALUE obj, VALUE max_seconds, VALUE max_bytes)
{
    GetJioFile(obj);
    Check_Type(max_seconds, T_FIXNUM);
    Check_Type(max_bytes, T_FIXNUM);
    TRAP_BEG;
    return (jfs_autosync_start(file->fs, (time_t)FIX2LONG(max_seconds), (size_t)FIX2LONG(max_bytes)) == 0) ? Qtrue : Qfalse;
    TRAP_END;
}

/*
 *  call-seq:
 *     file.stop_autosync    =>  boolean
 *
 *  Stops a previously started autosync thread.
 *
 * === Examples
 *     file.stop_autosync    =>  boolean
 *
*/

static VALUE rb_jio_file_stop_autosync(VALUE obj)
{
    GetJioFile(obj);
    TRAP_BEG;
    return (jfs_autosync_stop(file->fs) == 0) ? Qtrue : Qfalse;
    TRAP_END;
}

/*
 *  call-seq:
 *     file.read(10)    =>  String
 *
 *  Reads from a libjio file handle. Works just like UNIX read(2)
 *
 * === Examples
 *     file.read(10)    =>  String
 *
*/

static VALUE rb_jio_file_read(VALUE obj, VALUE length)
{
    ssize_t bytes;
    char *buf = NULL;
    ssize_t len;
    GetJioFile(obj);
    AssertLength(length);
    len = (ssize_t)FIX2LONG(length);
    buf = xmalloc(len + 1);
    if (buf == NULL) rb_memerror();
    TRAP_BEG;
    bytes = jread(file->fs, buf, len);
    TRAP_END;
    if (bytes == -1) {
       xfree(buf);
       rb_sys_fail("jread");
    }
    return JioEncode(rb_str_new(buf, (long)len));
}

/*
 *  call-seq:
 *     file.pread(10, 10)    =>  String
 *
 *  Reads from a libjio file handle at a given offset. Works just like UNIX pread(2)
 *
 * === Examples
 *     file.pread(10, 10)    =>  String
 *
*/

static VALUE rb_jio_file_pread(VALUE obj, VALUE length, VALUE offset)
{
    ssize_t bytes;
    char *buf = NULL;
    ssize_t len;
    GetJioFile(obj);
    AssertLength(length);
    AssertOffset(offset);
    len = (ssize_t)FIX2LONG(length);
    buf = xmalloc(len + 1);
    if (buf == NULL) rb_memerror();
    TRAP_BEG;
    bytes = jpread(file->fs, buf, len, (off_t)NUM2OFFT(offset));
    TRAP_END;
    if (bytes == -1) {
       xfree(buf);
       rb_sys_fail("jpread");
    }
    return JioEncode(rb_str_new(buf, (long)len));
}

/*
 *  call-seq:
 *     file.write("buffer")    =>  Fixnum
 *
 *  Writes to a libjio file handle. Works just like UNIX write(2)
 *
 * === Examples
 *     file.write("buffer")    =>  Fixnum
 *
*/

static VALUE rb_jio_file_write(VALUE obj, VALUE buf)
{
    ssize_t bytes;
    GetJioFile(obj);
    Check_Type(buf, T_STRING);
    TRAP_BEG;
    bytes = jwrite(file->fs, StringValueCStr(buf), (size_t)RSTRING_LEN(buf));
    TRAP_END;
    if (bytes == -1) rb_sys_fail("jwrite");
    return INT2NUM(bytes);
}

/*
 *  call-seq:
 *     file.pwrite("buffer", 10)    =>  Fixnum
 *
 *  Writes to a libjio file handle at a given offset. Works just like UNIX pwrite(2)
 *
 * === Examples
 *     file.pwrite("buffer", 10)    =>  Fixnum
 *
*/

static VALUE rb_jio_file_pwrite(VALUE obj, VALUE buf, VALUE offset)
{
    ssize_t bytes;
    GetJioFile(obj);
    Check_Type(buf, T_STRING);
    AssertOffset(offset);
    TRAP_BEG;
    bytes = jpwrite(file->fs, StringValueCStr(buf), (size_t)RSTRING_LEN(buf), (off_t)NUM2OFFT(offset));
    TRAP_END;
    if (bytes == -1) rb_sys_fail("jpwrite");
    return INT2NUM(bytes);
}

/*
 *  call-seq:
 *     file.lseek(10, JIO::SEEK_SET)    =>  Fixnum
 *
 *  Reposition the file pointer to the given offset, according to the whence directive.
 *
 * === Examples
 *     file.lseek(10, JIO::SEEK_SET)    =>  Fixnum
 *
*/

static VALUE rb_jio_file_lseek(VALUE obj, VALUE offset, VALUE whence)
{
    off_t off;
    GetJioFile(obj);
    AssertOffset(offset);
    Check_Type(whence, T_FIXNUM);
    TRAP_BEG;
    off = jlseek(file->fs, (off_t)NUM2OFFT(offset), FIX2INT(whence));
    TRAP_END;
    if (off == -1) rb_sys_fail("jlseek");
    return OFFT2NUM(off);
}

/*
 *  call-seq:
 *     file.truncate(10)    =>  Fixnum
 *
 *  Truncate the file to the given size.
 *
 * === Examples
 *     file.truncate(10)    =>  Fixnum
 *
*/

static VALUE rb_jio_file_truncate(VALUE obj, VALUE length)
{
    off_t len;
    GetJioFile(obj);
    AssertLength(length);
    TRAP_BEG;
    len = jtruncate(file->fs, (off_t)NUM2OFFT(length));
    TRAP_END;
    if (len == -1) rb_sys_fail("jtruncate");
    return OFFT2NUM(len);
}

/*
 *  call-seq:
 *     file.fileno    =>  Fixnum
 *
 *  Return the file descriptor number for the file.
 *
 * === Examples
 *     file.fileno    =>  Fixnum
 *
*/

static VALUE rb_jio_file_fileno(VALUE obj)
{
    int fd;
    GetJioFile(obj);
    TRAP_BEG;
    fd = jfileno(file->fs);
    TRAP_END;
    if (fd == -1) rb_sys_fail("jfileno");
    return INT2NUM(fd);
}

/*
 *  call-seq:
 *     file.rewind    =>  nil
 *
 *  Adjusts the file so that the next I/O operation will take place at the beginning of the file.
 *
 * === Examples
 *     file.rewind    =>  nil
 *
*/

static VALUE rb_jio_file_rewind(VALUE obj)
{
    GetJioFile(obj);
    TRAP_BEG;
    jrewind(file->fs);
    TRAP_END;
    return Qnil;
}

/*
 *  call-seq:
 *     file.tell    =>  Fixnum
 *
 *  Determine the current file offset.
 *
 * === Examples
 *     file.tell    =>  Fixnum
 *
*/

static VALUE rb_jio_file_tell(VALUE obj)
{
    long size;
    GetJioFile(obj);
    TRAP_BEG;
    size = jftell(file->fs);
    TRAP_END;
    if (size == -1) rb_sys_fail("jftell");
    return INT2NUM(size);
}

/*
 *  call-seq:
 *     file.eof?    =>  boolean
 *
 *  Check for end-of-file.
 *
 * === Examples
 *     file.eof?    =>  boolean
 *
*/

static VALUE rb_jio_file_eof_p(VALUE obj)
{
    GetJioFile(obj);
    TRAP_BEG;
    return (jfeof(file->fs) != 0) ? Qtrue : Qfalse;
    TRAP_END;
}

/*
 *  call-seq:
 *     file.error?    =>  boolean
 *
 *  Determines if an error condition has occurred.
 *
 * === Examples
 *     file.error?    =>  boolean
 *
*/

static VALUE rb_jio_file_error_p(VALUE obj)
{
    int res;
    GetJioFile(obj);
    TRAP_BEG;
    res = jferror(file->fs);
    TRAP_END;
    if (res == 0) return Qfalse;
    return INT2NUM(res);
}

/*
 *  call-seq:
 *     file.clearerr    =>  nil
 *
 *  Resets the error flag for this file, if any.
 *
 * === Examples
 *     file.clearerr    =>  nil
 *
*/

static VALUE rb_jio_file_clearerr(VALUE obj)
{
    GetJioFile(obj);
    TRAP_BEG;
    jclearerr(file->fs);
    TRAP_END;
    return Qnil;
}

void
Init_jio_ext()
{
    mJio = rb_define_module("JIO");

    cJioFile = rb_define_class_under(mJio, "File", rb_cObject);
    cJioTransaction = rb_define_class_under(mJio, "Transaction", rb_cObject);

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
    rb_define_module_function(mJio, "open", rb_jio_s_open, 4);
    rb_define_module_function(mJio, "check", rb_jio_s_check, 2);

/*
 *  JIO::File method definitions
 */
    rb_define_method(cJioFile, "sync", rb_jio_file_sync, 0);
    rb_define_method(cJioFile, "close", rb_jio_file_close, 0);
    rb_define_method(cJioFile, "move_journal", rb_jio_file_move_journal, 1);
    rb_define_method(cJioFile, "autosync", rb_jio_file_autosync, 2);
    rb_define_method(cJioFile, "stop_autosync", rb_jio_file_stop_autosync, 0);
    rb_define_method(cJioFile, "read", rb_jio_file_read, 1);
    rb_define_method(cJioFile, "pread", rb_jio_file_pread, 2);
    rb_define_method(cJioFile, "write", rb_jio_file_write, 1);
    rb_define_method(cJioFile, "pwrite", rb_jio_file_pwrite, 2);
    rb_define_method(cJioFile, "lseek", rb_jio_file_lseek, 2);
    rb_define_method(cJioFile, "truncate", rb_jio_file_truncate, 1);
    rb_define_method(cJioFile, "fileno", rb_jio_file_fileno, 0);
    rb_define_method(cJioFile, "rewind", rb_jio_file_rewind, 0);
    rb_define_method(cJioFile, "tell", rb_jio_file_tell, 0);
    rb_define_method(cJioFile, "eof?", rb_jio_file_eof_p, 0);
    rb_define_method(cJioFile, "error?", rb_jio_file_error_p, 0);
    rb_define_method(cJioFile, "clearerr", rb_jio_file_clearerr, 0);
    rb_define_method(cJioFile, "transaction", rb_jio_file_new_transaction, 1);

/*
 *  JIO::Transaction method definitions
 */
    rb_define_method(cJioTransaction, "read", rb_jio_transaction_read, 2);
    rb_define_method(cJioTransaction, "views", rb_jio_transaction_views, 0);
    rb_define_method(cJioTransaction, "write", rb_jio_transaction_write, 2);
    rb_define_method(cJioTransaction, "commit", rb_jio_transaction_commit, 0);
    rb_define_method(cJioTransaction, "rollback", rb_jio_transaction_rollback, 0);
    rb_define_method(cJioTransaction, "release", rb_jio_transaction_release, 0);
    rb_define_method(cJioTransaction, "committed?", rb_jio_transaction_committed_p, 0);
    rb_define_method(cJioTransaction, "rollbacked?", rb_jio_transaction_rollbacked_p, 0);
    rb_define_method(cJioTransaction, "rollbacking?", rb_jio_transaction_rollbacking_p, 0);
}