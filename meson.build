project('GexGfxRip', ['c'],
  version : '0.30',
  default_options : ['warning_level=1'])

platform = target_machine.system()
arch = target_machine.cpu_family()
cc = meson.get_compiler('c')


compiler_args = [
  '-Wno-compare-distinct-pointer-types',
  '-Wno-int-conversion',
  '-Wno-incompatible-pointer-types',
  '-Wno-pointer-integer-compare',
  '-Wno-deprecated-declarations'
]

if platform == 'windows'
  compiler_args += '-D_WIN32'
  compiler_args += '-D_CRT_SECURE_NO_WARNINGS'
endif

if target_machine.endian() == 'big'
  compiler_args += '-D__BIG_ENDIAN__'
endif

add_global_arguments(compiler_args, language : 'c')


# header_include_directories : libpng_inc

#TODO: OPTIONAL MINGW LIBRARIES
if platform == 'windows'
  if arch == 'x86'
    libsPath =  join_paths(meson.source_root(), 'vcpkg/installed/x86-windows-static')
  elif arch == 'x86_64'
    libsPath =  join_paths(meson.source_root(), 'vcpkg/installed/x64-windows-static')
  elif arch == 'aarch64'
    libsPath =  join_paths(meson.source_root(), 'vcpkg/installed/arm64-windows')
  endif

  zlib_dep = declare_dependency(
    dependencies : cc.find_library('zlib', dirs : join_paths(libsPath, 'lib')),
    include_directories : include_directories(join_paths(libsPath, 'include')),
  )
  libpng_dep = declare_dependency(
    dependencies : cc.find_library('libpng16', dirs : join_paths(libsPath, 'lib')),
    include_directories : include_directories(join_paths(libsPath, 'include')),
  )
else
  zlib_dep = dependency('zlib')
  libpng_dep = dependency('libpng')
endif

deps = [zlib_dep, libpng_dep]

src = [
  'src/main.c',
  'src/graphics/write_png.c',
  'src/graphics/gfx.c',
  'src/filescanning/filescanning.c',
  'src/filescanning/filescanning_tiles.c',
  'src/filescanning/filescanning_obj_gfx.c',
  'src/filescanning/fseeking_helper.c',
  'src/essentials/ptr_map.c',
  'src/essentials/vector.c',
  'src/essentials/stack.c'] 
exe = executable('gexgfxrip', src, dependencies : deps)

test('simple test', exe)