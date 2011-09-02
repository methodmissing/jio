Gem::Specification.new do |s|
  s.name = "jio"
  s.version = defined?(JIO) ? JIO::VERSION : (ENV['VERSION'] || '0.1')
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
  s.test_files = Dir['test/test_*.rb']
  s.add_development_dependency('rake-compiler', '~> 0.7.7')
end