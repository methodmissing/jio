#include <jio_ext.h>

/*
 *  Generic transaction error handler
 */
static inline VALUE rb_jio_transaction_result(ssize_t ret, const char *ctx)
{
    char err_buf[BUFSIZ];
    if (ret >= 0) return Qtrue;
    if (ret == -1) snprintf(err_buf, BUFSIZ, "JIO transaction error on %s (atomic warranties preserved)", ctx);
    if (ret == -2) snprintf(err_buf, BUFSIZ, "JIO transaction error on %s (atomic warranties broken)", ctx);
    rb_sys_fail(err_buf);
    return INT2NUM(ret);
}

/*
 *  GC callbacks for JIO::Transaction
 */
void rb_jio_mark_transaction(void *ptr)
{
    jio_jtrans_wrapper *trans = (jio_jtrans_wrapper *)ptr;
    rb_gc_mark(trans->views_ary);
}

static void rb_jio_free_transaction_views(jio_jtrans_wrapper *trans)
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

void rb_jio_free_transaction(void *ptr)
{
    jio_jtrans_wrapper *trans = (jio_jtrans_wrapper *)ptr;
    if(trans) {
        if (trans->trans != NULL && !(trans->flags & JIO_TRANSACTION_RELEASED)) jtrans_free(trans->trans);
        rb_jio_free_transaction_views(trans);
        xfree(trans);
    }
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
    JioGetTransaction(obj);
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
    JioGetTransaction(obj);
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
    JioGetTransaction(obj);
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
    JioGetTransaction(obj);
    TRAP_BEG;
    ret = jtrans_commit(trans->trans);
    TRAP_END;
    return rb_jio_transaction_result(ret, "commit");
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
    JioGetTransaction(obj);
    TRAP_BEG;
    ret = jtrans_rollback(trans->trans);
    TRAP_END;
    res = rb_jio_transaction_result(ret, "rollback");
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
    JioGetTransaction(obj);
    TRAP_BEG;
    jtrans_free(trans->trans);
    TRAP_END;
    trans->flags |= JIO_TRANSACTION_RELEASED;
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
    JioGetTransaction(obj);
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
    JioGetTransaction(obj);
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
    JioGetTransaction(obj);
    t = trans->trans;
    return (t->flags & J_ROLLBACKING) ? Qtrue : Qfalse;
}

void _init_rb_jio_transaction()
{
    rb_define_const(mJio, "J_NOLOCK", INT2NUM(J_NOLOCK));
    rb_define_const(mJio, "J_NOROLLBACK", INT2NUM(J_NOROLLBACK));
    rb_define_const(mJio, "J_LINGER", INT2NUM(J_LINGER));
    rb_define_const(mJio, "J_COMMITTED", INT2NUM(J_COMMITTED));
    rb_define_const(mJio, "J_ROLLBACKED", INT2NUM(J_ROLLBACKED));
    rb_define_const(mJio, "J_ROLLBACKING", INT2NUM(J_ROLLBACKING));
    rb_define_const(mJio, "J_RDONLY", INT2NUM(J_RDONLY));

    rb_cJioTransaction = rb_define_class_under(mJio, "Transaction", rb_cObject);

    rb_define_method(rb_cJioTransaction, "read", rb_jio_transaction_read, 2);
    rb_define_method(rb_cJioTransaction, "views", rb_jio_transaction_views, 0);
    rb_define_method(rb_cJioTransaction, "write", rb_jio_transaction_write, 2);
    rb_define_method(rb_cJioTransaction, "commit", rb_jio_transaction_commit, 0);
    rb_define_method(rb_cJioTransaction, "rollback", rb_jio_transaction_rollback, 0);
    rb_define_method(rb_cJioTransaction, "release", rb_jio_transaction_release, 0);
    rb_define_method(rb_cJioTransaction, "committed?", rb_jio_transaction_committed_p, 0);
    rb_define_method(rb_cJioTransaction, "rollbacked?", rb_jio_transaction_rollbacked_p, 0);
    rb_define_method(rb_cJioTransaction, "rollbacking?", rb_jio_transaction_rollbacking_p, 0);
}