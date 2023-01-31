compiler = meson.get_compiler('cpp')

digitaltwin = shared_library('digitaltwin', 'digitaltwin.cpp',
  dependencies: [
    dependency('threads')]
)

# executable('app', 'app.cpp',
#   link_with: digitaltwin,
#   dependencies: [
#     dependency('gtkmm-4.0'),
#     compiler.find_library('libboost_json'),
#     compiler.find_library('libboost_filesystem')]
# )
