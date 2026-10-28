project('gr-hle', 'cpp',
  default_options : ['cpp_std=c++20', 'warning_level=2', 'buildtype=release'])

# Gold Rush! on dos-hle-kit. The seam modules live in src/seams/ by area; add decompiled
# modules under src/decompiled/; src/support/ holds what is not a seam (font, debug aids) and list them here as the port grows.
hlekit = subproject('dos-hle-kit').get_variable('hlekit_dep')
project_inc = include_directories('src')   # #include "seams/...", "decompiled/..."

game_src = files(
  'src/game.cpp',
  'src/seams/seams.cpp',
  'src/seams/video.cpp',
  'src/seams/input.cpp',
  'src/seams/sound.cpp',
  'src/seams/timing.cpp',
  'src/seams/file.cpp',
  'src/seams/text.cpp',
  'src/seams/memory.cpp',
  'src/seams/platform.cpp',
  'src/support/debug.cpp',
)

executable('gr-hle', game_src,
  include_directories : project_inc, dependencies : [hlekit])
