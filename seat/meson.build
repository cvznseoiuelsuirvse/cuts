subdir('session')

libinput_dep = dependency('libinput')
libudev_dep = dependency('libudev')
libxkb_dep = dependency('xkbcommon')
libseat_dep = dependency('libseat')

seat_sources = files(
  'input.c',
) + session_sources

seat_dep = declare_dependency(
  sources: seat_sources,
  dependencies: [
    libseat_dep,
    libxkb_dep,
    libinput_dep, 
    libudev_dep
  ],
)
