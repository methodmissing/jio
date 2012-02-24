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

# Courtesy of EventMachine and @tmm1
def check_libs libs = [], fatal = false
  libs.all? { |lib| have_library(lib) || (abort("could not find library: #{lib}") if fatal) }
end

def check_heads heads = [], fatal = false
  heads.all? { |head| have_header(head) || (abort("could not find header: #{head}") if fatal)}
end

case RUBY_PLATFORM
when /mswin32/, /mingw32/, /bccwin32/
  check_heads(%w[windows.h winsock.h], true)
  check_libs(%w[kernel32 rpcrt4 gdi32], true)

  if GNU_CHAIN
    CONFIG['LDSHARED'] = "$(CXX) -shared -lstdc++"
  else
    $defs.push "-EHs"
    $defs.push "-GR"
  end

when /solaris/
  add_define 'OS_SOLARIS8'

  if CONFIG['CC'] == 'cc' and `cc -flags 2>&1` =~ /Sun/ # detect SUNWspro compiler
    # SUN CHAIN
    add_define 'CC_SUNWspro'
    $preload = ["\nCXX = CC"] # hack a CXX= line into the makefile
    $CFLAGS = CONFIG['CFLAGS'] = "-KPIC"
    CONFIG['CCDLFLAGS'] = "-KPIC"
    CONFIG['LDSHARED'] = "$(CXX) -G -KPIC -lCstd"
  else
    # GNU CHAIN
    # on Unix we need a g++ link, not gcc.
    CONFIG['LDSHARED'] = "$(CXX) -shared"
  end

when /openbsd/
  # OpenBSD branch contributed by Guillaume Sellier.

  # on Unix we need a g++ link, not gcc. On OpenBSD, linking against libstdc++ have to be explicitly done for shared libs
  CONFIG['LDSHARED'] = "$(CXX) -shared -lstdc++ -fPIC"
  CONFIG['LDSHAREDXX'] = "$(CXX) -shared -lstdc++ -fPIC"

when /darwin/
  # on Unix we need a g++ link, not gcc.
  # Ff line contributed by Daniel Harple.
  CONFIG['LDSHARED'] = "$(CXX) " + CONFIG['LDSHARED'].split[1..-1].join(' ')

when /aix/
  CONFIG['LDSHARED'] = "$(CXX) -shared -Wl,-G -Wl,-brtl"

else
  # on Unix we need a g++ link, not gcc.
  CONFIG['LDSHARED'] = "$(CXX) -shared"
end

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
if defined?(RUBY_ENGINE) && RUBY_ENGINE =~ /rbx/ && RUBY_PLATFORM =~ /linux/
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