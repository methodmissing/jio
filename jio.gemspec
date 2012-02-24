# encoding: utf-8

require File.expand_path('../lib/jio/version', __FILE__)

Gem::Specification.new do |s|
  s.name = "jio"
  s.version = JIO::VERSION
  s.summary = "jio - transactional, journaled file I/O for Ruby"
  s.description = "jio - transactional, journaled file I/O for Ruby"
  s.authors = ["Lourens Naudé"]
  s.email = ["lourens@methodmissing.com"]
  s.homepage = "http://github.com/methodmissing/jio"
  s.date = Time.now.utc.strftime('%Y-%m-%d')
  s.platform = Gem::Platform::RUBY
  s.extensions = Dir["ext/jio/extconf.rb"]
  s.has_rdoc = true
  s.files = `git ls-files`.split
  s.test_files = `git ls-files test`.split("\n")
  s.rdoc_options = ["--charset=UTF-8"]
  s.require_paths = ["lib"]
  s.add_development_dependency('rake-compiler', '~> 0.8.0')
end