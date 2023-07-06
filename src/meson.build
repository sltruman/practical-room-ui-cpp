os = build_machine.system()

compiler = meson.get_compiler('cpp')

libraries = [    
  dependency('threads'),
  dependency('gtkmm-4.0'),
  dependency('opencv4')]

if os == 'windows'
  libraries += [
    compiler.find_library('ws2_32'),
    compiler.find_library('libboost_filesystem-mt')]
else
  libraries += [
    compiler.find_library('libboost_filesystem')]
endif

run_command('glib-compile-resources', '--target=resources.c', '--generate-source', 'app.gresource.xml', check: true)

exe = executable('app', ['app.cpp','resources.c','digitaltwin.cpp'],
  dependencies: libraries)

test('simple test', exe)