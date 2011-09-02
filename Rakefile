# encoding: utf-8

require 'rake/extensiontask'
require 'rake/testtask'
begin
require 'rdoc/task'
rescue LoadError # fallback to older 1.8.7 rubies
require 'rake/rdoctask'
end

spec = eval(IO.read('jio.gemspec'))

task :compile => :build_libjio
task :clobber => :clobber_libjio

Rake::ExtensionTask.new('jio', spec) do |ext|
  ext.name = 'jio_ext'
  ext.ext_dir = 'ext/jio'
end

task :clobber_libjio do
  Dir.chdir "ext/libjio" do
    sh "make clean"
  end
end

task :build_libjio do
  Dir.chdir "ext/libjio" do
    sh "make"
  end unless File.exist?("ext/libjio/eio.o")
end

RDOC_FILES = FileList["README.rdoc", "ext/jio/jio_ext.c", "lib/jio.rb"]

Rake::RDocTask.new do |rd|
  rd.title = "jio - a libjio wrapper for Ruby"
  rd.main = "README.rdoc"
  rd.rdoc_dir = "doc"
  rd.rdoc_files.include(RDOC_FILES)
end

desc 'Run JIO tests'
Rake::TestTask.new(:test) do |t|
  t.pattern = "test/test_*.rb"
  t.verbose = true
  t.warning = true
end
task :test => :compile

task :default => :test
