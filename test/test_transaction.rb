# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestTransaction < JioTestCase
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