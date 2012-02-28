# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestFile < JioTestCase
  def test_open_close
    file = JIO.open(*OPEN_ARGS)
    assert_instance_of JIO::File, file
  ensure
    assert file.close
  end

  def test_sync
    file = JIO.open(*OPEN_ARGS)
    assert file.sync
  ensure
    assert file.close
  end

  def test_move_journal
    file = JIO.open(*OPEN_ARGS)
    assert file.move_journal(SANDBOX)
  ensure
    assert file.close
  end

  def test_autosync
    file = JIO.open(*OPEN_ARGS)
    assert file.autosync(1, 1024)
    file.write("CO")
    file.write("MM")
    file.write("IT")
    sleep 1.1
    file.rewind
    assert_equal "COMMIT", file.read(6)
    assert file.stop_autosync
  ensure
    assert file.close
  end

  def test_read_write
    file = JIO.open(*OPEN_ARGS)
    assert_equal "", file.read(0)
    assert_equal 4, file.write('ABCD')
    assert_equal 'ABCD', file.pread(4, 0)
    file.pwrite('EFGH', 4)
    assert_equal 'EFGH', file.pread(4, 4)
    file.pwrite('LMNOPQRS', 0)
    assert_equal 'LMNOPQRS', file.pread(8, 0)
  ensure
    assert file.close
  end

  def test_lseek
    file = JIO.open(*OPEN_ARGS)
    assert_equal 4, file.write('ABCD')
    file.lseek(2, IO::SEEK_SET)
    assert_equal 'CD', file.read(2)
  ensure
    assert file.close
  end

  def test_truncate
    file = JIO.open(*OPEN_ARGS)
    assert_equal 4, file.write('ABCD')
    assert_equal 4, File.size(FILE)
    file.truncate(2)
    assert_equal 2, File.size(FILE)
    file.truncate(0)
    assert_equal 0, File.size(FILE)
  ensure
    assert file.close
  end

  def test_fileno
    file = JIO.open(*OPEN_ARGS)
    assert_instance_of Fixnum, file.fileno
  ensure
    assert file.close
  end

  def test_rewind
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    file.rewind
    assert_equal 'ABCD', file.read(4)
  ensure
    assert file.close
  end

  def test_tell
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    assert_equal 4, file.tell
  ensure
    assert file.close
  end

  def test_eof_p
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    assert file.eof?
    file.rewind
    assert !file.eof?
  ensure
    assert file.close
  end

  def test_error
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    assert !file.error?
  ensure
    assert file.close
  end
end