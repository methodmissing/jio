# encoding: utf-8

require 'jio_ext'

module JIO
  VERSION = '0.1'

  class File
    alias orig_transaction transaction
    def transaction(flags)
      if block_given?
        begin
          trans = orig_transaction(flags)
          yield trans
        rescue
          trans.rollback
          error = true
          raise
        ensure
          trans.commit if !error && !trans.committed?
          trans.release
        end
      else
        orig_transaction(flags)
      end
    end
  end
end