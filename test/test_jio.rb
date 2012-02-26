# encoding: utf-8

require File.join(File.dirname(__FILE__), 'helper')

class TestJio < JioTestCase
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
end