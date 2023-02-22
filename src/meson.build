compiler = meson.get_compiler('cpp')

digitaltwin = shared_library('digitaltwin', 'digitaltwin.cpp',
  dependencies: [
    dependency('threads'),
    compiler.find_library('libboost_filesystem')]
)

run_command('glib-compile-resources', '--target=resources.c', '--generate-source', 'app.gresource.xml', check: true)

executable('app', ['app.cpp','resources.c'],
  link_with: digitaltwin,
  dependencies: [
    dependency('gtkmm-4.0'),
    compiler.find_library('libboost_filesystem')]
)
