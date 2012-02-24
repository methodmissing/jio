# encoding: utf-8

begin
  require "jio/jio_ext"
rescue LoadError
  require "jio_ext"
end

module JIO
end

require 'jio/file'