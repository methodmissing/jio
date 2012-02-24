# encoding: utf-8

module JIO
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