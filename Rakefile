# encoding: utf-8

require 'rubygems' unless defined?(Gem)
require 'rake' unless defined?(Rake)

require 'rake/extensiontask'
require 'rake/testtask'
begin
require 'rdoc/task'
rescue LoadError # fallback to older 1.8.7 rubies
require 'rake/rdoctask'
end

gemspec = eval(IO.read('jio.gemspec'))

Gem::PackageTask.new(gemspec) do |pkg|
end

Rake::ExtensionTask.new('jio', gemspec) do |ext|
  ext.name = 'jio_ext'
  ext.ext_dir = 'ext/jio'

  CLEAN.include 'ext/jio/dst'
  CLEAN.include 'ext/libjio'
  CLEAN.include 'lib/**/jio_ext.*'
end

Rake::RDocTask.new do |rd|
  files = FileList["README.rdoc", "lib/**/*.rb", "ext/jio/*.c"]
  rd.title = "jio - transactional, journaled file I/O for Ruby"
  rd.main = "README.rdoc"
  rd.rdoc_dir = "doc"
  rd.options << "--promiscuous"
  rd.rdoc_files.include(files)
end

desc 'Run jio tests'
Rake::TestTask.new(:test) do |t|
  t.test_files = Dir.glob("test/**/test_*.rb")
  t.verbose = true
  t.warning = true
end

task :test => :compile
task :default => :test
