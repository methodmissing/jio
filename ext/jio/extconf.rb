# encoding: utf-8

require 'mkmf'
require 'pathname'

def sys(cmd, err_msg)
  p cmd
  system(cmd) || fail(err_msg)
end

def fail(msg)
  STDERR.puts msg
  exit(1)
end

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']

# XXX fallbacks specific to Darwin for JRuby (does not set these values in RbConfig::CONFIG)
LIBEXT = RbConfig::CONFIG['LIBEXT'] || 'a'
DLEXT = RbConfig::CONFIG['DLEXT'] || 'bundle'

cwd = Pathname(File.expand_path(File.dirname(__FILE__)))
dst_path = cwd + 'dst'
libs_path = dst_path + 'lib'
vendor_path = cwd + '..'
libjio_path = vendor_path + 'libjio'
libjio_include_path = libjio_path + 'libjio'

# extract dependencies
unless File.directory?(libjio_path)
  fail "The 'tar' (creates and manipulates streaming archive files) utility is required to extract dependencies" if `which tar`.strip.empty?
  Dir.chdir(vendor_path) do
    sys "tar xvzf libjio.tar.gz", "Could not extract the libjio archive!"
  end
end

# build libjio
lib = libs_path + "libjio.#{LIBEXT}"
Dir.chdir libjio_path do
  sys "make PREFIX=#{dst_path} install", "libjio compile error!"
end unless File.exist?(lib)

dir_config('jio')

have_func('rb_thread_blocking_region')

$INCFLAGS << " -I#{libjio_include_path}"

$LIBPATH << libs_path.to_s

# Special case to prevent Rubinius compile from linking system libjio if present
if defined?(RUBY_ENGINE) && RUBY_ENGINE =~ /rbx/
  CONFIG['LDSHARED'] = "#{CONFIG['LDSHARED']} -Wl,-rpath=#{libs_path.to_s}"
end

fail "Error compiling and linking libjio" unless have_library("jio")

$defs << "-pedantic"
# libjio requires large file support
$defs << "-D_LARGEFILE_SOURCE=1"
$defs << "-D_LARGEFILE64_SOURCE=1"

$CFLAGS << ' -Wall -funroll-loops'
$CFLAGS << ' -Wextra -O0 -ggdb3' if ENV['DEBUG']

create_makefile('jio_ext')