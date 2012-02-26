# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestJio < JioTestCase
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
    file.close
  end

  def test_move_journal
    file = JIO.open(*OPEN_ARGS)
    assert file.move_journal(SANDBOX)
  ensure
    file.close
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
    file.close
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
    file.close
  end

  def test_lseek
    file = JIO.open(*OPEN_ARGS)
    assert_equal 4, file.write('ABCD')
    file.lseek(2, IO::SEEK_SET)
    assert_equal 'CD', file.read(2)
  ensure
    file.close
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
    file.close
  end

  def test_fileno
    file = JIO.open(*OPEN_ARGS)
    assert_equal 3, file.fileno
  ensure
    file.close
  end

  def test_rewind
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    file.rewind
    assert_equal 'ABCD', file.read(4)
  ensure
    file.close
  end

  def test_tell
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    assert_equal 4, file.tell
  ensure
    file.close
  end

  def test_eof_p
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    assert file.eof?
    file.rewind
    assert !file.eof?
  ensure
    file.close
  end

  def test_error
    file = JIO.open(*OPEN_ARGS)
    file.write('ABCD')
    assert !file.error?
  ensure
    file.close
  end

  def test_spawn_transaction
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    assert_instance_of JIO::Transaction, trans
  ensure
    trans.release
    file.close
  end

  def test_spawn_transaction_with_block
    file = JIO.open(*OPEN_ARGS)
    file.transaction(JIO::J_LINGER) do |trans|
      assert_instance_of JIO::Transaction, trans
      trans.write("COMMIT", 0)
    end
    assert_equal "COMMIT", file.read(6)
  ensure
    file.close
  end

  def test_spawn_transaction_with_block_error
    file = JIO.open(*OPEN_ARGS)
    file.transaction(JIO::J_LINGER) do |trans|
      assert_instance_of JIO::Transaction, trans
      trans.write("COMMIT", 0)
      raise "rollback"
    end rescue nil
    assert_not_equal "COMMIT", file.read(6)
  ensure
    file.close
  end

  def test_transaction_views
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    trans.write('COMMIT', 0)
    trans.read(2, 2)
    assert_equal [], trans.views
    assert_equal nil, trans.release
  ensure
    file.close
  end

  def test_commit_transaction
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    trans.write('COMMIT', 0)
    trans.read(2, 2)
    trans.read(2, 4)
    assert_equal [], trans.views 
    assert trans.commit
    assert_equal %w(MM IT), trans.views
  ensure
    trans.release
    file.close
  end

  def test_rollback_transaction
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    assert trans.rollback
  ensure
    trans.release
    file.close
  end

  def test_transaction_committed_p
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    trans.write('COMMIT', 0)
    assert trans.commit
    assert trans.committed?
  ensure
    trans.release
    file.close
  end

  def test_check
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    trans.write('COMMIT', 0)
    assert trans.commit
    assert trans.committed?
    expected = {:reapplied=>1,
     :invalid=>2,
     :corrupt=>0,
     :total=>3,
     :in_progress=>0,
     :broken=>0}
    assert_equal expected, JIO.check(FILE, 0)
  ensure
    trans.release
    file.close
  end

  def test_transaction_rollbacked_p
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    trans.write('COMMIT', 0)
    assert trans.commit
    assert_equal 'COMMIT', file.read(6)
    assert trans.rollback
    assert trans.rollbacked?
  ensure
    trans.release
    file.close
  end

  def test_transaction_rollbacking_p
    file = JIO.open(*OPEN_ARGS)
    trans = file.transaction(JIO::J_LINGER)
    trans.write('COMMIT', 0)
    assert trans.commit
    assert trans.rollback
    assert !trans.rollbacking?
  ensure
    trans.release
    file.close
  end
end