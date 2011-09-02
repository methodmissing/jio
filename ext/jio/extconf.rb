require 'mkmf'

# http://rickvanderzwet.blogspot.com/2007/10/ever-wanted-to-build-shared-library-and.html

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']

dir_config('jio')

$defs << "-DRUBY_VM" if have_func('rb_thread_blocking_region')
libs_path = File.expand_path(File.join(File.dirname(__FILE__), '../libjio/.libs'))
find_library("jio", "jopen", libs_path)
$defs << "-pedantic"

create_makefile('jio_ext')