#include <jio_ext.h>

/*
 *  GC callbacks for JIO::File
 */
static void rb_jio_mark_file(void *ptr)
{
    jfs_wrapper *file = (jfs_wrapper *)ptr;
}

static void rb_jio_free_file(void *ptr)
{
    jfs_wrapper *file = (jfs_wrapper *)ptr;
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

static VALUE rb_jio_s_open(JIO_UNUSED VALUE jio, VALUE path, VALUE flags, VALUE mode, VALUE jflags)
{
    VALUE obj;
    jfs_wrapper *file = NULL;
    Check_Type(path, T_STRING);
    Check_Type(flags, T_FIXNUM);
    Check_Type(mode, T_FIXNUM);
    Check_Type(jflags, T_FIXNUM);
    obj = Data_Make_Struct(rb_cJioFile, jfs_wrapper, rb_jio_mark_file, rb_jio_free_file, file);
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

VALUE rb_jio_file_new_transaction(VALUE obj, VALUE flags)
{
    VALUE transaction;
    jtrans_wrapper *trans = NULL;
    GetJioFile(obj);
    Check_Type(flags, T_FIXNUM);
    transaction = Data_Make_Struct(rb_cJioTransaction, jtrans_wrapper, rb_jio_mark_transaction, rb_jio_free_transaction, trans);
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

void _init_rb_jio_file()
{
    rb_define_module_function(mJio, "open", rb_jio_s_open, 4);

    rb_cJioFile = rb_define_class_under(mJio, "File", rb_cObject);

    rb_define_method(rb_cJioFile, "sync", rb_jio_file_sync, 0);
    rb_define_method(rb_cJioFile, "close", rb_jio_file_close, 0);
    rb_define_method(rb_cJioFile, "move_journal", rb_jio_file_move_journal, 1);
    rb_define_method(rb_cJioFile, "autosync", rb_jio_file_autosync, 2);
    rb_define_method(rb_cJioFile, "stop_autosync", rb_jio_file_stop_autosync, 0);
    rb_define_method(rb_cJioFile, "read", rb_jio_file_read, 1);
    rb_define_method(rb_cJioFile, "pread", rb_jio_file_pread, 2);
    rb_define_method(rb_cJioFile, "write", rb_jio_file_write, 1);
    rb_define_method(rb_cJioFile, "pwrite", rb_jio_file_pwrite, 2);
    rb_define_method(rb_cJioFile, "lseek", rb_jio_file_lseek, 2);
    rb_define_method(rb_cJioFile, "truncate", rb_jio_file_truncate, 1);
    rb_define_method(rb_cJioFile, "fileno", rb_jio_file_fileno, 0);
    rb_define_method(rb_cJioFile, "rewind", rb_jio_file_rewind, 0);
    rb_define_method(rb_cJioFile, "tell", rb_jio_file_tell, 0);
    rb_define_method(rb_cJioFile, "eof?", rb_jio_file_eof_p, 0);
    rb_define_method(rb_cJioFile, "error?", rb_jio_file_error_p, 0);
    rb_define_method(rb_cJioFile, "clearerr", rb_jio_file_clearerr, 0);
    rb_define_method(rb_cJioFile, "transaction", rb_jio_file_new_transaction, 1);
}