# encoding: utf-8

require 'test/unit'
require 'jio'
require 'fileutils' #19
require 'timeout'

TMP = File.expand_path(File.join(File.dirname(__FILE__), '..', 'tmp'))
SANDBOX = File.join(TMP, 'sandbox')
FileUtils.rm_rf SANDBOX
FileUtils.mkdir_p SANDBOX

class JioTestCase < Test::Unit::TestCase
  FILE = File.join(SANDBOX, 'file.jio')
  OPEN_ARGS = [FILE, JIO::RDWR | JIO::CREAT | JIO::TRUNC, 0600, JIO::J_LINGER]

  undef_method :default_test if method_defined? :default_test

  def setup
    File.unlink(FILE) if File.exists?(FILE)
  end

  if ENV['STRESS_GC']
    def setup
      GC.stress = true
    end

    def teardown
      GC.stress = false
    end
  end
end