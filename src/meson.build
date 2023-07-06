compiler = meson.get_compiler('cpp')

digitaltwin = shared_library('digitaltwin', 'digitaltwin.cpp',
  dependencies: [
    dependency('threads'),
    compiler.find_library('ws2_32', required : false),
    compiler.find_library('libboost_filesystem-mt')
    ]
)

run_command('glib-compile-resources', '--target=resources.c', '--generate-source', 'app.gresource.xml', check: true)

executable('app', ['app.cpp','resources.c'],
  link_with: digitaltwin,
  dependencies: [
    dependency('gtkmm-4.0'),
    dependency('opencv4'),
    compiler.find_library('ws2_32', required : false)
    ]
)
